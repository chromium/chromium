// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {extractAnnotatedPageContent} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/annotated_page_content_extraction.js';
import {getRemoteFrameRemoteToken, MAX_APC_NODE_DEPTH, MAX_APC_RESPONSE_DEPTH, NONCE_ATTR} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/common.js';
import type {PageContent} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/page_content_types.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// Cache JSON.stringify to protect against website overrides.
const JSONStringify = JSON.stringify;

// Direct references to `Element`/`HTMLElement` members, used to read shadow
// roots and query descendants without going through instance properties that a
// hostile page could clobber (e.g. a form control named `querySelectorAll` or
// `shadowRoot`).
const shadowRootGetter =
    Object.getOwnPropertyDescriptor(Element.prototype, 'shadowRoot')?.get;
const querySelectorAllMethod = Element.prototype.querySelectorAll;

// Returns the equivalent of `element.shadowRoot` but reads directly from the
// `Element` prototype to prevent clobbering. Only open shadow roots are
// reachable from script; closed shadow roots return null.
function safeShadowRoot(element: Element): ShadowRoot|null {
  return (shadowRootGetter ? shadowRootGetter.call(element) :
                             element.shadowRoot) as ShadowRoot |
      null;
}

// Returns all descendants of `element` matching `selector`, calling the
// `Element` prototype method directly to prevent clobbering.
function safeQuerySelectorAllAsArray(
    element: Element, selector: string): Element[] {
  return Array.from(querySelectorAllMethod.call(element, selector));
}

// Model that contains the link data for anchor tags that are extracted.
interface LinkData {
  href: string;
  linkText: string | null;
}

// Model that represents a frame on a different origin from the main frame that
// can't have its content extracted which is identified with a `remoteToken` to
// let the browser graft the content for this frame later on.
interface CrossOriginFrameData {
  remoteToken: string;
}

// Model that contains the frame data of a frame on the same origin as the
// parent frame (this frame). This can be the main frame itself.
interface SameOriginFrameData {
  currentNodeInnerText: string;
  children: RemoteFrameData[];
  sourceUrl: string;
  title: string;
  links?: LinkData[];
}

// Data extracted on remote frames (other than the main frame) on the same
// or different origin.
type RemoteFrameData = SameOriginFrameData | CrossOriginFrameData;

// Model for when the page context should be detached.
interface DetachData {
  shouldDetachPageContext: boolean;
}

// The result of the extraction can be one of the following types. `null` is
// used if extraction failed for some reason.
type ExtractionResult = SameOriginFrameData|DetachData|PageContent|string|null;

// Returns true if the page context should be detached, false otherwise. The
// logic is defined in the placeholder replacement.
function shouldDetachPageContext(): boolean {
  // This statement is replaced by a function block during placeholder
  // replacement. See
  // ios/chrome/browser/intelligence/proto_wrappers/page_context_extractor_java_script_feature.mm.
  // Falls back to false by default if no value can be provided from the call.
  return (window as any).gCrWebPlaceholderPageContextShouldDetach() ?? false;
}

/**
 * Returns true if page context IPC optimization is enabled.
 */
function isPageContextIPCOptimizationEnabled() {
  return (window as any).gCrWebPlaceholderPageContextIPCOptimization ?? false;
}

// Keep this implementation in sync with `getInnerTextIncludingShadowDom()` in
// page_context_wrapper.mm.
// Collects the innerText of a node, additionally piercing open Shadow DOM
// roots whose text is not reflected in Element.innerText. Nested shadow roots
// are handled through the recursion. Closed shadow roots are inaccessible from
// script and are skipped. Shadow roots are accessed through clobbering-safe
// helpers so a hostile page cannot break extraction.
function getInnerTextIncludingShadowDom(node: Element): string {
  const texts: string[] = [];
  const nodeText = (node as HTMLElement).innerText;
  if (nodeText) {
    texts.push(nodeText);
  }
  // The node itself and its light-DOM descendants may be shadow hosts.
  // querySelectorAll does not cross shadow boundaries, so shadow hosts nested
  // inside another shadow tree are reached via the recursion.
  const hosts: Element[] = safeQuerySelectorAllAsArray(node, '*');
  if (safeShadowRoot(node)) {
    hosts.unshift(node);
  }
  for (const host of hosts) {
    const shadowRoot = safeShadowRoot(host);
    if (!shadowRoot) {
      continue;
    }
    // Iterate all child nodes (not just elements) so text authored directly
    // under the shadow root (e.g. `root.textContent = 'hi'`) is not dropped.
    for (const shadowChild of shadowRoot.childNodes) {
      let shadowText = '';
      if (shadowChild.nodeType === Node.ELEMENT_NODE) {
        shadowText = getInnerTextIncludingShadowDom(shadowChild as Element);
      } else if (shadowChild.nodeType === Node.TEXT_NODE) {
        shadowText = (shadowChild.textContent ?? '').trim();
      }
      if (shadowText) {
        texts.push(shadowText);
      }
    }
  }
  // `innerText` above collects all light-DOM text at once. Shadow-DOM chunks
  // are therefore appended after it rather than inserted at their visual
  // positions.
  return texts.join('\n');
}

// Recursively constructs the innerText tree for the passed node and its
// children iframes. Content extraction can only take place on nodes on the same
// origin as the main frame which is represented by SameOriginFrameData. Iframes
// on a different origin that can't have their content extracted from here are
// represented by CrossOriginFrameData that contains a token that allows the
// browser grafting the frame content later. Returns null if extraction can't be
// done.
const constructInnerTextTree =
  (node: HTMLElement | null, frameURL: string, frameTitle: string,
    nonceAttributeValue: string, keepCrossOriginFrameData: boolean): SameOriginFrameData | null => {
    // Early return if the node is null, not an HTMLElement, or already
    // processed.
    if (!node || node.getAttribute(NONCE_ATTR) === nonceAttributeValue) {
      return null;
    }

    // Mark node as processed.
    node.setAttribute(NONCE_ATTR, nonceAttributeValue);

    // Get all nested iframes within the current node.
    const nestedIframes = node.getElementsByTagName('iframe');
    const childNodeInnerTexts = [...nestedIframes].map((iframe) => {
      if (!iframe) {
        return null;
      }

      // Try to access the iframe's body, failure is possible (cross-origin
      // iframes).
      let iframeBody: HTMLElement | null = null;
      let iframeTitle: string = '';
      let contentDoc = null;
      try {
        contentDoc = iframe.contentDocument;
        if (contentDoc) {
          iframeBody = contentDoc.body;
          iframeTitle = contentDoc.title;
        }
      } catch (error) {
      }

      if (!contentDoc) {
        return keepCrossOriginFrameData ? {
          remoteToken: getRemoteFrameRemoteToken(iframe),
        } as CrossOriginFrameData :
          null;
      }

      // Recursively construct the innerText tree for the iframe's body.
      return iframeBody ?
        constructInnerTextTree(
          iframeBody, iframe.src, iframeTitle, nonceAttributeValue,
          keepCrossOriginFrameData) :
        null;
    });

    const result: SameOriginFrameData = {
      currentNodeInnerText: getInnerTextIncludingShadowDom(node),
      children: childNodeInnerTexts.filter((item) => item !== null) as
          RemoteFrameData[],
      sourceUrl: frameURL,
      title: frameTitle,
    };

    // Add all the frame's anchor tags to a links array with their HREF/URL
    // and textContent.
    const linksArray: LinkData[] = [];
    const anchorElements =
      node.querySelectorAll<HTMLAnchorElement>('a[href]');
    anchorElements.forEach((anchor) => {
      linksArray.push({
        href: (anchor instanceof SVGAElement) ? anchor.href.baseVal :
          anchor.href,
        linkText: anchor.textContent,
      });
    });
    result.links = linksArray;

    return result;
  };

// Extracts the page context in a tree structure starting from the document body
// as the root, and recursively traverses through same-origin nested iframes to
// retrieve their context as well, constructing the tree structure. iframes are
// marked as processed with a nonce to avoid double extracting the context from
// frames, but only for the current run. Early returns if the PageContext should
// be detached, or the frame is not the top-most same-origin frame.
function extractPageContext(
    nonce: string, keepCrossOriginFrameData: boolean,
    useRichExtraction: boolean, actionableMode: boolean,
    extractPaidContent: boolean, attemptPaidContentJsonFixing: boolean,
    includeSensitivePaymentsForRedaction: boolean,
    extractAutofillOtpRedactions: boolean): ExtractionResult {
  // If the PageContext should be detached, early return.
  if (shouldDetachPageContext()) {
    return { shouldDetachPageContext: true } as DetachData;
  }

  // The script should only run if it has no same-origin parent. (The script
  // should only start execution on top-most nodes of a given origin).
  if (window.self !== window.top &&
      location.ancestorOrigins?.[0] === location.origin) {
    // Not the top-most same-origin frame, early exit.
    return null;
  }

  if (useRichExtraction) {
    // We reserve 1 depth unit to account for the wrapping `PageContent` object.
    // The `PageContent` object itself adds one level of nesting to the
    // structure parsed by `ValueResultFromWKResult` on the native side.
    const maxDepth = MAX_APC_RESPONSE_DEPTH - MAX_APC_NODE_DEPTH;
    const apc = extractAnnotatedPageContent(
        document, nonce, 0, maxDepth, actionableMode, extractPaidContent,
        attemptPaidContentJsonFixing, includeSensitivePaymentsForRedaction,
        extractAutofillOtpRedactions);
    return isPageContextIPCOptimizationEnabled() ? JSONStringify(apc) : apc;
  }

  // Recursively constructs the tree from the root node.
  const innerTextTree = constructInnerTextTree(
      document.body, window.location.href, document.title, nonce,
      keepCrossOriginFrameData);
  return isPageContextIPCOptimizationEnabled() ? JSONStringify(innerTextTree) :
                                                 innerTextTree;
}

const pageExtractorApi = new CrWebApi('pageContextExtractor');

pageExtractorApi.addFunction('extractPageContext', extractPageContext);

gCrWeb.registerApi(pageExtractorApi);
