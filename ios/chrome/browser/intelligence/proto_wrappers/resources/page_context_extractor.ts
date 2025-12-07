// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

// Model that contains the link data for anchor tags that are extracted.
interface LinkData {
  href: string;
  linkText: string|null;
}

// Model that contains the frame data for the frames that are extracted.
interface FrameData {
  currentNodeInnerText: string;
  children: FrameData[];
  sourceURL: string;
  title: string;
  links?: LinkData[];
}

// Model for when the page context should be detached.
interface DetachData {
  shouldDetachPageContext: boolean;
}

// The result of the extraction can be one of the following types.
type ExtractionResult = FrameData|DetachData|null;

// A function will be defined here by the placeholder replacement.
// It will be called to determine if the page context should be detached.
declare const SHOULD_DETACH_PAGE_CONTEXT: () => boolean;

/*! {{PLACEHOLDER_FOR_DETACH_LOGIC}} */

const NONCE_ATTR = 'data-__gCrWeb-innerText-processed';

// Recursively constructs the innerText tree for the passed node and its
// children same-origin iframes.
const constructSameOriginInnerTextTree =
    (node: HTMLElement|null, frameURL: string, frameTitle: string,
     nonceAttributeValue: string, includeAnchors: boolean): FrameData|null => {
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
        let iframeBody: HTMLElement|null = null;
        let iframeTitle: string = '';
        try {
          const contentDoc = iframe.contentDocument;
          iframeBody = contentDoc ? contentDoc.body : null;
          iframeTitle = contentDoc ? contentDoc.title : '';
        } catch (error) {
          return null;
        }

        // Recursively construct the innerText tree for the iframe's body.
        return iframeBody ? constructSameOriginInnerTextTree(
                                iframeBody, iframe.src, iframeTitle,
                                nonceAttributeValue, includeAnchors) :
                            null;
      });

      const result: FrameData = {
        currentNodeInnerText: node.innerText,
        children: childNodeInnerTexts.filter((item) => item !== null) as
            FrameData[],
        sourceURL: frameURL,
        title: frameTitle,
      };

      if (includeAnchors) {
        // Add all the frame's anchor tags to a links array with their HREF/URL
        // and textContent.
        const linksArray: LinkData[] = [];
        const anchorElements =
            node.querySelectorAll<HTMLAnchorElement>('a[href]');
        anchorElements.forEach((anchor) => {
          linksArray.push({
            href: anchor.href,
            linkText: anchor.textContent,
          });
        });
        result.links = linksArray;
      }

      return result;
    };

// Extracts the page context in a tree structure starting from the document body
// as the root, and recursively traverses through same-origin nested iframes to
// retrieve their context as well, constructing the tree structure. iframes are
// marked as processed with a nonce to avoid double extracting the context from
// frames, but only for the current run. Early returns if the PageContext should
// be detached, or the frame is not the top-most same-origin frame.
function extractPageContext(
    includeAnchors: boolean, nonce: string): ExtractionResult {
  // If the PageContext should be detached, early return.
  if (SHOULD_DETACH_PAGE_CONTEXT()) {
    return {shouldDetachPageContext: true};
  }

  // The script should only run if it has no same-origin parent. (The script
  // should only start execution on top-most nodes of a given origin to
  // correctly reconstruct the tree structure).
  if (window.self !== window.top &&
      location.ancestorOrigins?.[0] === location.origin) {
    return null;
  }

  // Recursively constructs the tree from the root node.
  return constructSameOriginInnerTextTree(
      document.body, window.location.href, document.title, nonce,
      includeAnchors);
}

const pageExtractorApi = new CrWebApi();

pageExtractorApi.addFunction('extractPageContext', extractPageContext);

gCrWeb.registerApi('pageContextExtractor', pageExtractorApi);
