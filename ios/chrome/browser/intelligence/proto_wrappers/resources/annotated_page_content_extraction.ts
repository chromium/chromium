// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {APC_NODE_DEPTH_COST, getRemoteFrameRemoteToken, MAX_APC_RESPONSE_DEPTH, NONCE_ATTR} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/common.js';
import {getNodeId, getOrCreateNodeId} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/dom_node_ids.js';
import {FormControlType, PageContentAnchorRel, PageContentAnnotatedRole, PageContentAttributeType, PageContentRedactionDecision, PageContentTableRowType, PageContentTextSize} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/page_content_types.js';
import type {PageContent, PageContentAttributes, PageContentFormControlData, PageContentFormData, PageContentFrameData, PageContentFrameInteractionInfo, PageContentNode, PageContentPageInteractionInfo} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/page_content_types.js';

// Set of DOM Node IDs that are considered interactive (focused, selection
// start/end). These nodes should be included in the APC tree even if they are
// generic containers.
type InteractiveNodeIds = Set<number>;

// The last known pointer position.
// Tags that we fundamentally do not support or that contain non-content data.
const TAG_STYLE = 'STYLE';
const TAG_SCRIPT = 'SCRIPT';
const TAG_NOSCRIPT = 'NOSCRIPT';
const TAG_TEMPLATE = 'TEMPLATE';
const TAG_AUDIO = 'AUDIO';
const TAG_APPLET = 'APPLET';
const TAG_EMBED = 'EMBED';
const TAG_OBJECT = 'OBJECT';
const TAG_DATALIST = 'DATALIST';
const TAG_HEAD = 'HEAD';

// Media tags.
const TAG_SVG = 'SVG';
const TAG_CANVAS = 'CANVAS';
const TAG_VIDEO = 'VIDEO';

// Common tags.
const TAG_IFRAME = 'IFRAME';
const TAG_IMG = 'IMG';
const TAG_A = 'A';
const TAG_TABLE = 'TABLE';
const TAG_TR = 'TR';
const TAG_TD = 'TD';
const TAG_TH = 'TH';
const TAG_FORM = 'FORM';
const TAG_INPUT = 'INPUT';
const TAG_TEXTAREA = 'TEXTAREA';
const TAG_SELECT = 'SELECT';
const TAG_BUTTON = 'BUTTON';
const TAG_P = 'P';
const TAG_OL = 'OL';
const TAG_UL = 'UL';
const TAG_DL = 'DL';
const TAG_LI = 'LI';
const TAG_DT = 'DT';
const TAG_DD = 'DD';
const TAG_FIGURE = 'FIGURE';

// Tags with annotated role.
const TAG_HEADER = 'HEADER';
const TAG_NAV = 'NAV';
const TAG_SEARCH = 'SEARCH';
const TAG_MAIN = 'MAIN';
const TAG_ARTICLE = 'ARTICLE';
const TAG_SECTION = 'SECTION';
const TAG_ASIDE = 'ASIDE';
const TAG_FOOTER = 'FOOTER';

// Heading tags.
const TAG_H1 = 'H1';
const TAG_H2 = 'H2';
const TAG_H3 = 'H3';
const TAG_H4 = 'H4';
const TAG_H5 = 'H5';
const TAG_H6 = 'H6';

// Tags that should be rejected during the DOM tree walk.
const TAGS_TO_REJECT = [
  TAG_STYLE,
  TAG_SCRIPT,
  TAG_NOSCRIPT,
  TAG_TEMPLATE,
  TAG_AUDIO,
  TAG_APPLET,
  TAG_EMBED,
  TAG_OBJECT,
  TAG_DATALIST,
  TAG_HEAD,
];

// Tags that contain valid content but are not yet extracted.
// TODO(crbug.com/468852704): Remove tags from this list as they are
// implemented.
const TAGS_TO_SUPPORT_EVENTUALLY = [TAG_SVG];

// Tags that should be strictly rejected if they are invisible,
// because they are considered "leaf" nodes.
const TAGS_TO_STRICTLY_REJECT_IF_HIDDEN = [
  TAG_IMG,
  TAG_IFRAME,
  TAG_CANVAS,
  TAG_SVG,
  TAG_VIDEO,
  TAG_INPUT,
  TAG_TEXTAREA,
  TAG_SELECT,
  TAG_BUTTON,
];

// Regex to get the characters to capitalize.
const TEXT_TRANSFORM_CAPITALIZE_REGEX = /\b\w/g;

// Characters used for text masking, depending on the style.
const TEXT_MASKING_CHAR_DISC = '\u2022';    // disc/default (•)
const TEXT_MASKING_CHAR_CIRCLE = '\u25E6';  // ◦
const TEXT_MASKING_CHAR_SQUARE = '\u25A0';  // ■

// The constant length of the masked text.
const MASKED_TEXT_LENGTH = 7;

// Form control types.
const PASSWORD_TYPE = 'password';
const BUTTON_TYPE = 'button';
const CHECKBOX_TYPE = 'checkbox';
const COLOR_TYPE = 'color';
const DATE_TYPE = 'date';
const DATETIME_LOCAL_TYPE = 'datetime-local';
const EMAIL_TYPE = 'email';
const FILE_TYPE = 'file';
const HIDDEN_TYPE = 'hidden';
const IMAGE_TYPE = 'image';
const MONTH_TYPE = 'month';
const NUMBER_TYPE = 'number';
const RADIO_TYPE = 'radio';
const RANGE_TYPE = 'range';
const RESET_TYPE = 'reset';
const SEARCH_TYPE = 'search';
const SUBMIT_TYPE = 'submit';
const TELEPHONE_TYPE = 'tel';
const TIME_TYPE = 'time';
const URL_TYPE = 'url';
const WEEK_TYPE = 'week';
const TEXT_TYPE = 'text';

const BASIC_CONTENT_ATTRIBUTES: PageContentAttributes = {
  attributeType: PageContentAttributeType.UNKNOWN,
  annotatedRoles: [],
  isAdRelated: false,
};

/**
 * Maps a tag name to its corresponding PageContentAnnotatedRole.
 *
 * @param tagName The tag name to map.
 * @return The corresponding PageContentAnnotatedRole, or null if no mapping
 *     exists.
 */
function getAnnotatedRoleForTag(tagName: string): PageContentAnnotatedRole|
    null {
  // Matches AnnotatedRole enum in
  // components/optimization_guide/proto/features/common_quality_data.proto.
  switch (tagName) {
    case TAG_HEADER:
      return PageContentAnnotatedRole.HEADER;
    case TAG_NAV:
      return PageContentAnnotatedRole.NAV;
    case TAG_SEARCH:
      return PageContentAnnotatedRole.SEARCH;
    case TAG_MAIN:
      return PageContentAnnotatedRole.MAIN;
    case TAG_ARTICLE:
      return PageContentAnnotatedRole.ARTICLE;
    case TAG_SECTION:
      return PageContentAnnotatedRole.SECTION;
    case TAG_ASIDE:
      return PageContentAnnotatedRole.ASIDE;
    case TAG_FOOTER:
      return PageContentAnnotatedRole.FOOTER;
    default:
      return null;
  }
}

// Constants for text size categorization, mirroring Blink's
// third_party/blink/renderer/modules/content_extraction/ai_page_content_agent.cc.
const HEADING_1_FONT_SIZE_MULTIPLIER = 2.0;
const HEADING_3_FONT_SIZE_MULTIPLIER = 1.17;
const HEADING_5_FONT_SIZE_MULTIPLIER = 0.83;
const HEADING_6_FONT_SIZE_MULTIPLIER = 0.67;

/**
 * Determines the text size category based on the font size. Returns
 * PageContentTextSize.M if the relative font size can't be computed.
 * Ratios are based on browser defaults for headings, which are as follows:
 *
 * Heading 1: 2em
 * Heading 2: 1.5em
 * Heading 3: 1.17em
 * Heading 4: 1em
 * Heading 5: 0.83em
* Heading 6: 0.67em
 *
 * @param fontSize The font size string (e.g., "16px").
 * @param doc The document to use for root font size reference.
 * @return The corresponding PageContentTextSize category.
 */
function getTextSizeCategory(
  fontSize: string, doc: Document): PageContentTextSize {
  const size = parseFloat(fontSize);
  if (isNaN(size)) {
    return PageContentTextSize.M;
  }

  const rootStyle = doc.defaultView?.getComputedStyle(doc.documentElement);
  if (!rootStyle) {
    return PageContentTextSize.M;
  }

  const docFontSize = parseFloat(rootStyle.fontSize);
  if (isNaN(docFontSize) || docFontSize <= 0) {
    return PageContentTextSize.M;
  }

  const multiplier = size / docFontSize;

  if (multiplier >= HEADING_1_FONT_SIZE_MULTIPLIER) {
    return PageContentTextSize.XL;
  } else if (multiplier >= HEADING_3_FONT_SIZE_MULTIPLIER) {
    return PageContentTextSize.L;
  } else if (multiplier >= HEADING_5_FONT_SIZE_MULTIPLIER) {
    return PageContentTextSize.M;
  } else if (multiplier >= HEADING_6_FONT_SIZE_MULTIPLIER) {
    return PageContentTextSize.S;
  } else {
    return PageContentTextSize.XS;
  }
}

/**
 * Maps an element to its corresponding FormControlType.
 *
 * @param element The element to map.
 * @return The corresponding FormControlType.
 */
function getFormControlType(element: HTMLElement): FormControlType|undefined {
  const tagName = element.tagName;

  if (tagName === TAG_BUTTON) {
    const type = (element as HTMLButtonElement).type;
    switch (type) {
      case 'submit':
        return FormControlType.BUTTON_SUBMIT;
      case 'reset':
        return FormControlType.BUTTON_RESET;
      case 'button':
      default:
        return FormControlType.BUTTON_BUTTON;
    }
  }

  if (tagName === TAG_INPUT) {
    const type = (element as HTMLInputElement).type;
    switch (type) {
      case BUTTON_TYPE:
        return FormControlType.INPUT_BUTTON;
      case CHECKBOX_TYPE:
        return FormControlType.INPUT_CHECKBOX;
      case COLOR_TYPE:
        return FormControlType.INPUT_COLOR;
      case DATE_TYPE:
        return FormControlType.INPUT_DATE;
      case DATETIME_LOCAL_TYPE:
        return FormControlType.INPUT_DATETIME_LOCAL;
      case EMAIL_TYPE:
        return FormControlType.INPUT_EMAIL;
      case FILE_TYPE:
        return FormControlType.INPUT_FILE;
      case HIDDEN_TYPE:
        return FormControlType.INPUT_HIDDEN;
      case IMAGE_TYPE:
        return FormControlType.INPUT_IMAGE;
      case MONTH_TYPE:
        return FormControlType.INPUT_MONTH;
      case NUMBER_TYPE:
        return FormControlType.INPUT_NUMBER;
      case PASSWORD_TYPE:
        return FormControlType.INPUT_PASSWORD;
      case RADIO_TYPE:
        return FormControlType.INPUT_RADIO;
      case RANGE_TYPE:
        return FormControlType.INPUT_RANGE;
      case RESET_TYPE:
        return FormControlType.INPUT_RESET;
      case SEARCH_TYPE:
        return FormControlType.INPUT_SEARCH;
      case SUBMIT_TYPE:
        return FormControlType.INPUT_SUBMIT;
      case TELEPHONE_TYPE:
        return FormControlType.INPUT_TELEPHONE;
      case TIME_TYPE:
        return FormControlType.INPUT_TIME;
      case URL_TYPE:
        return FormControlType.INPUT_URL;
      case WEEK_TYPE:
        return FormControlType.INPUT_WEEK;
      case TEXT_TYPE:
      default:
        // Standard default type when no type is specified.
        return FormControlType.INPUT_TEXT;
    }
  }

  if (tagName === TAG_SELECT) {
    if ((element as HTMLSelectElement).multiple) {
      return FormControlType.SELECT_MULTIPLE;
    }
    return FormControlType.SELECT_ONE;
  }

  if (tagName === TAG_TEXTAREA) {
    return FormControlType.TEXT_AREA;
  }

  // Fallback, though we shouldn't reach here for form controls.
  return undefined;
}

/**
 * Extracts the relationships (rel attribute) from an anchor element.
 *
 * @param anchorElement The anchor element to extract relationships from.
 * @return An array of PageContentAnchorRel, defaulting to [RELATION_UNKNOWN] if
 *     none found.
 */
function getAnchorRel(anchorElement: HTMLAnchorElement):
    PageContentAnchorRel[] {
  const rels = anchorElement.relList;
  if (!rels) {
    return [PageContentAnchorRel.RELATION_UNKNOWN];
  }

  const result: PageContentAnchorRel[] = [];
  if (rels.contains('noopener')) {
    result.push(PageContentAnchorRel.RELATION_NO_OPENER);
  }
  if (rels.contains('noreferrer')) {
    result.push(PageContentAnchorRel.RELATION_NO_REFERRER);
  }
  if (rels.contains('opener')) {
    result.push(PageContentAnchorRel.RELATION_OPENER);
  }
  if (rels.contains('privacy-policy')) {
    result.push(PageContentAnchorRel.RELATION_PRIVACY_POLICY);
  }
  if (rels.contains('terms-of-service')) {
    result.push(PageContentAnchorRel.RELATION_TERMS_OF_SERVICE);
  }

  if (result.length === 0) {
    result.push(PageContentAnchorRel.RELATION_UNKNOWN);
  }
  return result;
}

/**
 * Extracts form specific content attributes from a given DOM element.
 *
 * @param form The form element to process.
 * @return The populated PageContentFormData.
 */
function getFormData(form: HTMLFormElement): PageContentFormData {
  const formData: PageContentFormData = {};
  if (form.name) {
    formData.formName = form.name;
  }
  if (form.action) {
    formData.actionUrl = form.action;
  }
  return formData;
}


// TODO(crbug.com/480945289): Complete this function as more data becomes
// available throughout iterations.
/**
 * Determines if an element should be treated as a generic container.
 * This is a fallback classification for elements that don't match specific
 * types but have structural significance (e.g., scrolling, positioning).
 *
 * @param element The element to check.
 * @return True if the element is a generic container, false otherwise.
 */
function isGenericContainer(
    element: HTMLElement, interactiveNodeIds: InteractiveNodeIds): boolean {
  // Check if the element is an interactive node.
  const nodeId = getNodeId(element);
  if (nodeId !== null && interactiveNodeIds.has(nodeId)) {
    return true;
  }

  // A <figure> element is a semantic container for self-contained content, like
  // images or diagrams, making it a generic container.
  if (element.tagName === TAG_FIGURE) {
    return true;
  }

  // Elements with fixed or sticky positioning are removed from the normal flow
  // and often act as containers for UI elements like headers or sidebars.
  const windowObj = element.ownerDocument?.defaultView;
  if (!windowObj) {
    return false;
  }
  const style = windowObj.getComputedStyle(element);
  const position = style.position;
  if (position === 'fixed' || position === 'sticky') {
    return true;
  }

  // TODO(crbug.com/480945289): Add searches for Top/ViewTransitionLayer,
  // InteractionInfo (when enabled), and Labels (when enabled).


  // Scrollable elements act as containers because they create a new
  // visual context for their content, handling overflow.
  const overflowX = style.getPropertyValue('overflow-x');
  const overflowY = style.getPropertyValue('overflow-y');
  if (overflowX === 'auto' || overflowX === 'scroll' || overflowY === 'auto' ||
      overflowY === 'scroll') {
    return true;
  }

  return false;
}

/**
 * Extracts the frame interaction info (selection).
 *
 * @param document The document to extract data from.
 * @return The populated PageContentFrameInteractionInfo.
 */
function extractFrameInteractionInfo(document: Document):
    PageContentFrameInteractionInfo {
  const frameInteractionInfo: PageContentFrameInteractionInfo = {};
  const selection = document.getSelection();
  if (selection && selection.rangeCount > 0 && !selection.isCollapsed) {
    const range = selection.getRangeAt(0);

    const startNodeId = getOrCreateNodeId(range.startContainer);
    const endNodeId = getOrCreateNodeId(range.endContainer);

    if (endNodeId !== null && startNodeId !== null) {
      frameInteractionInfo.selection = {
        startDomNodeId: startNodeId,
        startOffset: range.startOffset,
        endDomNodeId: endNodeId,
        endOffset: range.endOffset,
        selectedText: selection.toString(),
      };
    }
  }
  return frameInteractionInfo;
}

// TODO(crbug.com/468854910): Add missing fields for PageContentFrameData:
// HTML metaData, containsPaidContent, and popup (if possible).
/**
 * Extracts data about the frame/document.
 *
 * @param document The document to extract data from.
 * @return The populated PageContentFrameData.
 */
function extractFrameData(document: Document): PageContentFrameData {
  const frameData: PageContentFrameData = {
    frameInteractionInfo: {},
    metaData: [],
    title: document.title || '',
    sourceUrl: document.URL,
  };

  frameData.frameInteractionInfo = extractFrameInteractionInfo(document);

  return frameData;
}

/**
 * Applies text transform and masking to the text content.
 * Matches Blink's LayoutText::TransformedText() behavior.
 *
 * @param text The original text content.
 * @param style The computed style of the parent element.
 * @return The transformed and masked text.
 */
function applyTextTransformAndMasking(
    text: string, style: CSSStyleDeclaration): string {
  let maskedText = text;

  // 1. Text Transform
  const transform = style.textTransform;
  if (transform) {
    if (transform === 'uppercase') {
      maskedText = maskedText.toUpperCase();
    } else if (transform === 'lowercase') {
      maskedText = maskedText.toLowerCase();
    } else if (transform === 'capitalize') {
      maskedText = maskedText.replace(
          TEXT_TRANSFORM_CAPITALIZE_REGEX, (char) => char.toUpperCase());
    }
  }

  // 2. Text Masking (-webkit-text-security).
  // This property is not in the standard CSSStyleDeclaration type.
  const masking = (style as any).webkitTextSecurity;
  if (masking && masking !== 'none') {
    let maskChar = TEXT_MASKING_CHAR_DISC;
    if (masking === 'circle') {
      maskChar = TEXT_MASKING_CHAR_CIRCLE;
    } else if (masking === 'square') {
      maskChar = TEXT_MASKING_CHAR_SQUARE;
    }
    return maskChar.repeat(MASKED_TEXT_LENGTH);
  }

  return maskedText;
}

/**
 * Generates attributes for a text node.
 *
 * @param domNode The text node to process.
 * @return The populated PageContentAttributes or null if content is empty.
 */
function getAttributesForTextNode(domNode: Node): PageContentAttributes|null {
  const textContent = domNode.textContent;
  if (!textContent) {
    return null;
  }

  const parentElement = domNode.parentElement;
  if (!parentElement) {
    // Can't compute the attributes for a text node that doesn't have a parent
    // which is unexpected.
    return null;
  }

  const windowObj = parentElement.ownerDocument?.defaultView;
  if (!windowObj) {
    return null;
  }
  const style = windowObj.getComputedStyle(parentElement);
  const maskedText = applyTextTransformAndMasking(textContent, style);

  if (maskedText.trim().length === 0) {
    // TODO(crbug.com/480945289): Use the white-space-collapse style when
    // deemed safe to use.
    // Handle "whitespace-only" (as per CSS spec : \n, \t, \s) text content.
    const whiteSpace = style.whiteSpace;
    if (['normal', 'nowrap'].includes(whiteSpace)) {
      // Whitespace-only text content that is all collapsed is not interesting.
      return null;
    }
  }

  const weight = style.fontWeight;
  const hasEmphasis = weight === 'bold' || weight === '700' ||
      parseInt(weight) >= 700 || style.fontStyle === 'italic';
  const textSize = domNode.ownerDocument ?
    getTextSizeCategory(style.fontSize, domNode.ownerDocument) :
    PageContentTextSize.M;
  return {
    attributeType: PageContentAttributeType.TEXT,
    annotatedRoles: [],
    isAdRelated: false,
    textInfo: {
      textContent: maskedText,
      textStyle: {
        textSize: textSize,
        hasEmphasis,
        // TODO(crbug.com/474935853): Add text color extraction.
        color: 0,
      },
    },
  };
}

// TODO(crbug.com/476341187): Carry status information when the max depth is
// reached.
/**
 * Processes an iframe element, including recursive extraction for same-origin
 * frames.
 *
 * @param iframeElement The iframe element to process.
 * @param nonce A unique identifier for the extraction run.
 * @param depth The current recursion depth.
 * @param maxDepth The maximum recursion depth.
 * @return The populated PageContentNode for the iframe.
 */
function getContentForIframeNode(
    iframeElement: HTMLIFrameElement, nonce: string, depth: number,
    maxDepth: number): PageContentNode {
  const attributes: PageContentAttributes = {
    attributeType: PageContentAttributeType.IFRAME,
    annotatedRoles: [],
    isAdRelated: false,
  };

  let childTree: PageContentNode|null = null;
  let localFrameData: PageContentFrameData|undefined;

  // Always register the frame to get a remote token, even for same-origin
  // frames. This allows identification of the frame document in the browser.
  const remoteToken = getRemoteFrameRemoteToken(iframeElement);

  try {
    const contentDoc = iframeElement.contentDocument;
    if (contentDoc && contentDoc.body) {
      // Recurse to start a new tree walk on the iframe content when available
      // (i.e. when on the same origin) because the TreeWalker doesn't walk
      // through iframe content.
      const pageContent = extractAnnotatedPageContent(
          contentDoc, nonce, depth + APC_NODE_DEPTH_COST, maxDepth);
      if (pageContent) {
        childTree = pageContent.rootNode;
        localFrameData = pageContent.frameData;
      }
    }
  } catch (error) {
    // Ignore errors when accessing the iframe content.
  }

  if (!localFrameData) {
    localFrameData = {
      title: iframeElement.title || '',
      sourceUrl: iframeElement.src,
      frameInteractionInfo: {},
      metaData: [],
      containsPaidContent: false,
    };
  }

  // Set the document ID for the frame data.
  localFrameData.documentId = remoteToken;


  // TODO(crbug.com/468857979): Add `redactedFrameMetadata` to the `content`
  // data once there is an option to only extract iframe data on the same
  // site (domain and one level of subdomain). Only populate the remote token if
  // grafting is needed to get the iframe content.
  attributes.iframeData = {
    frameToken: {value: childTree ? '' : remoteToken},
    content: {
      localFrameData: localFrameData,
    },
    // TODO(crbug.com/476402215): Evaluate setting remote token for iframes with
    // contentDocument but no body in the case the body is set later on.
  };

  const contentNode: PageContentNode = {
    childrenNodes: [],
    contentAttributes: attributes,
  };

  if (childTree) {
    // Add the iframe page child tree in the content tree if the iframe content
    // is accessible (i.e. on same origin frames).
    contentNode.childrenNodes.push(childTree);
  }
  return contentNode;
}

/**
 * Extracts form control specific content attributes from a given DOM element.
 * Handles inputs, textareas, selects, and buttons.
 *
 * @param domNode The element to process.
 * @param tagName The tag name of the element.
 * @return The populated PageContentFormControlData.
 */
function getFormControlData(
    domNode: HTMLElement, tagName: string): PageContentFormControlData {
  // There must be a type returned, throw an exception if not.
  const formControlType = getFormControlType(domNode)!;
  const formControlData: PageContentFormControlData = {
    formControlType: formControlType,
    selectOptions: [],
    isChecked: false,
    isRequired: false,
    // TODO(crbug.com/485211722): Set redaction decision for Autofill.
    redactionDecision: PageContentRedactionDecision.NO_REDACTION_NECESSARY,
  };

  const name = (domNode as HTMLInputElement).name;
  if (name !== undefined && name !== '') {
    formControlData.fieldName = name;
  }

  const value = (domNode as HTMLInputElement).value;
  if (value !== undefined) {
    // TODO(crbug.com/485211722): Complete implementation once redaction
    // decision is fully available.
    // Exclude password field value mirroring Blink's logic.
    // For now, only extract value if type != password.
    // TAG_TEXTAREA and TAG_SELECT do not support the 'type' attribute to
    // designate a password field, so we consider their values safe to extract
    // (unless they are custom passwords, which is handled separately).
    if (tagName !== TAG_INPUT ||
        (domNode as HTMLInputElement).type !== PASSWORD_TYPE) {
      formControlData.fieldValue = value;
    }
  }

  formControlData.isRequired = (domNode as HTMLInputElement).required ?? false;

  // Handle aria-required override.
  if (!formControlData.isRequired &&
      domNode.getAttribute('aria-required') === 'true') {
    formControlData.isRequired = true;
  }

  const isReadonly = (domNode as HTMLInputElement).readOnly ?? false;
  formControlData.isReadonly = isReadonly;

  // Handle aria-readonly override.
  if (!isReadonly && domNode.getAttribute('aria-readonly') === 'true') {
    formControlData.isReadonly = true;
  }

  // Checkbox and Radio.
  if (tagName === TAG_INPUT) {
    const inputElement = domNode as HTMLInputElement;
    if (inputElement.type === 'checkbox' || inputElement.type === 'radio') {
      formControlData.isChecked = inputElement.checked;
    }
  }

  // Placeholder.
  const placeholder = (domNode as HTMLInputElement).placeholder;
  if (placeholder) {
    formControlData.placeholder = placeholder;
  } else {
    const ariaPlaceholder = domNode.getAttribute('aria-placeholder');
    if (ariaPlaceholder) {
      formControlData.placeholder = ariaPlaceholder;
    }
  }

  // Select Options.
  if (tagName === TAG_SELECT) {
    const selectElement = domNode as HTMLSelectElement;
    for (const option of Array.from(selectElement.options)) {
      let text = option.text;
      if (!text) {
        text = option.label;
      }
      formControlData.selectOptions.push({
        value: option.value,
        text: text,
        isSelected: option.selected,
        disabled: option.disabled,
      });
    }
  }

  return formControlData as PageContentFormControlData;
}

/**
 * Returns basic content for an element node that is not a generic
 * container based on its tag name.
 *
 * @param domNode The element to process.
 * @param nonce Unique identifier for the extraction run.
 * @param depth Current recursion depth.
 * @return The populated PageContentNode or null if no basic match found.
 */
function getBasicContentForNonGenericElement(
    domNode: HTMLElement, nonce: string, depth: number,
    maxDepth: number): PageContentNode|null {
  const tagName = domNode.tagName;

  switch (tagName) {
    // 1. Complex Elements.
    case TAG_IFRAME:
      return getContentForIframeNode(
          domNode as HTMLIFrameElement, nonce, depth, maxDepth);
    case TAG_IMG:
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.IMAGE,
          imageInfo: {
            imageCaption: (domNode as HTMLImageElement).alt ?? '',
            // TODO(crbug.com/474936659): Include image source origin.
          },
        },
      };
    case TAG_A:
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.ANCHOR,
          anchorData: {
            url: (domNode as HTMLAnchorElement).href,
            rel: getAnchorRel(domNode as HTMLAnchorElement),
          },
        },
      };
    case TAG_CANVAS: {
      const rect = domNode.getBoundingClientRect();
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.CANVAS,
          canvasData: {
            layoutSize: {
              width: rect.width,
              height: rect.height,
            },
          },
        },
      };
    }
    case TAG_VIDEO: {
      const videoElement = domNode as HTMLVideoElement;
      // Use currentSrc if present (can be populated natively or by <source>
      // children). Fallback to src (even if empty string) to match Blink parity
      // where URL is extracted.
      const url = videoElement.currentSrc || videoElement.src || '';
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.VIDEO,
          videoData: {
            url: url,
            // TODO(crbug.com/382558422): Include video source origin.
          },
        },
      };
    }

    // 2. Structural & Layout Elements.
    case TAG_TABLE:
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.TABLE,
        },
      };
    case TAG_TR: {
      let rowType = PageContentTableRowType.BODY;
      // Use closest to find the nearest table section or table ancestor.
      // This handles cases where TR might be nested in a generic container
      // within a section. We include 'table' to ensure we stop at the nearest
      // table boundary and don't match a section from an outer table if this
      // row is inside a nested table.
      const section = domNode.closest('thead, tfoot, table');
      if (section && section.tagName === 'THEAD') {
        rowType = PageContentTableRowType.HEADER;
      } else if (section && section.tagName === 'TFOOT') {
        rowType = PageContentTableRowType.FOOTER;
      }
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.TABLE_ROW,
          tableRowData: {
            rowType: rowType,
          },
        },
      };
    }
    case TAG_TD:
    case TAG_TH:
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.TABLE_CELL,
        },
      };
    case TAG_FORM:
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.FORM,
          formData: getFormData(domNode as HTMLFormElement),
        },
      };
    case TAG_INPUT:
    case TAG_TEXTAREA:
    case TAG_SELECT:
    case TAG_BUTTON: {
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.FORM_CONTROL,
          formControlData: getFormControlData(domNode, tagName),
        },
      };
    }
    case TAG_H1:
    case TAG_H2:
    case TAG_H3:
    case TAG_H4:
    case TAG_H5:
    case TAG_H6:
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.HEADING,
        },
      };
    case TAG_P:
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.PARAGRAPH,
        },
      };
    case TAG_OL:
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.ORDERED_LIST,
        },
      };
    case TAG_UL:
    case TAG_DL:
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.UNORDERED_LIST,
        },
      };
    case TAG_LI:
    case TAG_DT:
    case TAG_DD:
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.LIST_ITEM,
        },
      };

    default:
      break;
  }
  return null;
}

// TODO(crbug.com/468852704): Extract the missing data to reach parity with
// third_party/blink/renderer/modules/content_extraction/
// ai_page_content_agent.cc.
/**
 * Generates content for an element node.
 *
 * @param domNode The element to process.
 * @param nonce Unique identifier for the extraction run.
 * @param depth Current recursion depth.
 * @return The populated PageContentNode or null if element should be
 *     skipped.
 */
function getContentForElementNode(
    domNode: HTMLElement, nonce: string, depth: number, maxDepth: number,
    interactiveNodeIds: InteractiveNodeIds): PageContentNode|null {
  let contentNode: PageContentNode|null = null;

  // 1. Try to get basic content for non-generic elements.
  contentNode =
      getBasicContentForNonGenericElement(domNode, nonce, depth, maxDepth);

  // 2. Fallback: Generic Container.
  if (!contentNode && isGenericContainer(domNode, interactiveNodeIds)) {
    contentNode = {
      childrenNodes: [],
      contentAttributes: {
        attributeType: PageContentAttributeType.CONTAINER,
        annotatedRoles: [],
        isAdRelated: false,
      },
    };
  }

  // TODO(crbug.com/468852704): Populate the rest of the attributes on top of
  // `basicAttributes`.

  return contentNode;
}

/**
 * Adds an annotated role to the attributes if applicable for the element's tag.
 *
 * @param element The element to check.
 * @param attributes The attributes object to populate.
 */
function addAnnotatedRoles(
    domNode: HTMLElement, attributesToPopulate: PageContentAttributes) {
  const role = getAnnotatedRoleForTag(domNode.tagName);
  if (role !== null) {
    attributesToPopulate.annotatedRoles = [role];
  }
}

// TODO(crbug.com/476341187): Carry status information when the max depth is
// reached.
/**
 * Generates a PageContentNode for a given DOM node if it contains valid
 * content. DOM node IDs are only generated and assigned if content can be
 * generated for the `domNode`.
 *
 * @param domNode The DOM node to process (Element or Text).
 * @param nonce Unique identifier for the extraction run.
 * @param depth Current recursion depth.
 * @param maxDepth Maximal depth for json objects beyond which content is
 *     truncated.
 * @return A new PageContentNode if valid content was found, null otherwise.
 */
function maybeGenerateContentNode(
    domNode: Node, nonce: string, depth: number, maxDepth: number,
    interactiveNodeIds: InteractiveNodeIds): PageContentNode|null {
  let contentAttributes: PageContentAttributes|null = null;
  if (domNode.nodeType === Node.TEXT_NODE) {
    contentAttributes = getAttributesForTextNode(domNode);
    if (contentAttributes) {
      const domNodeId = getOrCreateNodeId(domNode);
      if (domNodeId !== null) {
        contentAttributes.domNodeId = domNodeId;
      }
      return {
        childrenNodes: [],
        contentAttributes: contentAttributes,
      };
    }
  } else if (domNode.nodeType === Node.ELEMENT_NODE) {
    const element = domNode as HTMLElement;
    const contentNode = getContentForElementNode(
        element, nonce, depth, maxDepth, interactiveNodeIds);
    if (contentNode) {
      const domNodeId = getOrCreateNodeId(domNode);
      if (domNodeId !== null) {
        contentNode.contentAttributes.domNodeId = domNodeId;
      }
      addAnnotatedRoles(element, contentNode.contentAttributes);
      return contentNode;
    }
  }

  return null;
}

/**
 * Checks if a node should be accepted for extraction.
 */
function shouldAcceptNode(node: Node): number {
  if (node.nodeType === Node.ELEMENT_NODE) {
    const element = node as Element;
    if (TAGS_TO_REJECT.includes(element.tagName) ||
        TAGS_TO_SUPPORT_EVENTUALLY.includes(element.tagName)) {
      return NodeFilter.FILTER_REJECT;
    }
    const windowObj = element.ownerDocument?.defaultView;
    if (!windowObj) {
      return NodeFilter.FILTER_REJECT;
    }
    const style = windowObj.getComputedStyle(element);
    if (style.display === 'none') {
      // Ignore the nodes and all their descendants that do not have
      // any display style which means that they would not have a
      // corresponding LayoutObject in blink.
      return NodeFilter.FILTER_REJECT;
    }
    if (style.visibility === 'hidden') {
      // Strictly skip invisible leaf nodes.
      if (TAGS_TO_STRICTLY_REJECT_IF_HIDDEN.includes(element.tagName)) {
        return NodeFilter.FILTER_REJECT;
      }
      // For containers, we OPTIMISTICALLY ACCEPT (FILTER_ACCEPT).
      // They will be pruned later if they contain no visible content.
      return NodeFilter.FILTER_ACCEPT;
    }
  } else if (node.nodeType === Node.TEXT_NODE) {
    const parent = node.parentElement;
    // Determine the text node visibility based on their parent since
    // this is the best proxy we have for that due to the lack of
    // `getComputedStyle()` for text element nodes as opposed to the
    // LayoutObject in blink.
    const windowObj = parent?.ownerDocument?.defaultView;
    if (!windowObj) {
      return NodeFilter.FILTER_REJECT;
    }
    const style = windowObj.getComputedStyle(parent);
    if (style.display === 'none' || style.visibility === 'hidden') {
      return NodeFilter.FILTER_REJECT;
    }
  }
  return NodeFilter.FILTER_ACCEPT;
}

// Item in the ancestor stack.
interface AncestorStackItem {
  // Node object in the DOM tree.
  domNode: Node;
  // Node object in the APC tree.
  apcNode: PageContentNode;
  // Depth of the node in the APC tree.
  depth: number;
  // Whether the node has style.
  isVisible: boolean;
}

/**
 * Generates and adds apc content for the `node` as a child of the closest
 * valid ancestor in the `parentStackItem` if applicable. Pushes the new node as
 * the new parent in the `ancestorStack`.
 *
 * @param node The node to process.
 * @param nonce Unique identifier for the extraction run.
 * @param maxDepth The maximum recursion depth.
 * @param ancestorStack The stack of ancestors that provides the parent node and
 *     where the new node is pushed as the next closest parent.
 */
function generateAndPushContentNode(
    node: Node, nonce: string, maxDepth: number,
    ancestorStack: AncestorStackItem[],
    interactiveNodeIds: InteractiveNodeIds) {
  const parentStackItem = ancestorStack[ancestorStack.length - 1]!;

  // 2. Generate Content Node. Skip nodes that are too deep while keep
  // walking the tree since future nodes might be shallow enough.
  const currentDepth = parentStackItem.depth + APC_NODE_DEPTH_COST;
  if (currentDepth > maxDepth) {
    // Ignore the node if it exceeds the max depth.
    return;
  }

  const newApcNode = maybeGenerateContentNode(
      node, nonce, currentDepth, maxDepth, interactiveNodeIds);
  if (!newApcNode) {
    // Ignore the node if it can't be parsed. That node cannot be a parent
    // either where another node in the ancestor stack will be picked as the
    // parent for the descendents of this node (if there are).
    return;
  }

  parentStackItem.apcNode.childrenNodes.push(newApcNode);

  if (node.nodeType !== Node.ELEMENT_NODE) {
    return;
  }

  // Re-check visibility for stack logic.
  const element = node as Element;
  const windowObj = element.ownerDocument?.defaultView;
  let isVisible = true;
  if (windowObj) {
    const style = windowObj.getComputedStyle(element);
    // Blink treats 'opacity: 0' as visible. Only 'visibility:
    // hidden/collapse' is invisible.
    isVisible = style.visibility === 'visible';
  }

  ancestorStack.push({
    domNode: node,
    apcNode: newApcNode,
    depth: currentDepth,
    isVisible: isVisible,
  });
}

// TODO(crbug.com/485799759): Assess if we need the mouse position.
/**
 * Extracts the page interaction info (focus, pointer position).
 *
 * @param document The document to extract data from.
 * @return The populated PageContentPageInteractionInfo.
 */
function extractPageInteractionInfo(document: Document):
    PageContentPageInteractionInfo {
  const pageInteractionInfo: PageContentPageInteractionInfo = {};
  const activeElement = document.activeElement;
  if (activeElement) {
    const focusedId = getOrCreateNodeId(activeElement);
    if (focusedId !== null) {
      pageInteractionInfo.focusedDomNodeId = focusedId;
    }
  }
  return pageInteractionInfo;
}

/**
 * Gets the interactive nodes in the `document` (focused element, selection
 * start/end).
 *
 * @param document The document to extract data from.
 * @return The set of interactive node ids.
 */
function getInteractiveNodeIds(document: Document): InteractiveNodeIds {
  const interactiveNodeIds: InteractiveNodeIds = new Set();
  const focusedElement = document.activeElement;
  if (focusedElement) {
    const id = getOrCreateNodeId(focusedElement);
    if (id !== null) {
      interactiveNodeIds.add(id);
    }
  }
  const selection = document.getSelection();
  if (selection && selection.rangeCount > 0 && !selection.isCollapsed) {
    const range = selection.getRangeAt(0);
    const startId = getOrCreateNodeId(range.startContainer);
    const endId = getOrCreateNodeId(range.endContainer);
    if (startId !== null) {
      interactiveNodeIds.add(startId);
    }
    if (endId !== null) {
      interactiveNodeIds.add(endId);
    }
  }
  return interactiveNodeIds;
}

// TODO(crbug.com/485796293): Wrap this in a class.
/**
 * Extracts the annotated page content of the document starting from the body
 * as the root node. Uses a TreeWalker to read the nodes via an iterative
 * depth-first, pre-order traversal. Starts other TreeWalker instances
 * recursively on the visited iframe nodes whose content is accessible (i.e. on
 * the same origin) but not covered by the main TreeWalker, while keeping the
 * structure of the page.
 *
 * @param document The document to extract content from.
 * @param nonce A unique identifier for the current extraction run, used to mark
 *     processed iframes.
 * @param depth The current depth of the recursion. Will stop extraction if the
 *     depth limit is reached.
 * @return The extracted annotated page content, or null if the depth limit is
 *     reached or body is missing.
 */
export function extractAnnotatedPageContent(
    document: Document, nonce: string, depth: number = 0,
    maxDepth: number = MAX_APC_RESPONSE_DEPTH): PageContent|null {
  if (depth > maxDepth) {
    return null;
  }

  const root = document.body;
  if (!root) {
    return null;
  }

  // Do not extract the same content twice for the same nonce.
  if (root.getAttribute(NONCE_ATTR) === nonce) {
    return null;
  }
  root.setAttribute(NONCE_ATTR, nonce);

  const domNodeId = getOrCreateNodeId(root);
  if (domNodeId === null) {
    // If the root node can't be assigned an ID, it can't be processed.
    return null;
  }

  const rootNode: PageContentNode = {
    contentAttributes: {
      domNodeId: domNodeId,
      attributeType: PageContentAttributeType.ROOT,
      annotatedRoles: [],
      isAdRelated: false,
    },
    childrenNodes: [],
  };



  // Stack to track the current ancestry chain. At this point it is known that
  // that there is at least a root node that is walkable.
  // We use this to find the correct parent for the current node without
  // needing a full map of all visited nodes.
  const ancestorStack: AncestorStackItem[] =
      [{domNode: root, apcNode: rootNode, depth, isVisible: true}];

  // Collect interactive nodes (focused element, selection start/end).
  const interactiveNodeIds = getInteractiveNodeIds(document);

  const walker = document.createTreeWalker(
      root, NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT, {
        acceptNode: (node) => shouldAcceptNode(node),
      });

  let currentNode = walker.nextNode();
  while (currentNode) {
    // 1. Maintain Stack Invariant & Post-Pruning.
    // Prune (pop) the stack until the top of the stack is an ancestor of the
    // current node so the stack only contains the ancestors of the current
    // node's subtree. The stack size cannot go below 1 (root node) to make
    // sure that all the walked nodes have a parent. The root node is used as
    // the default parent for all the walked nodes (even if they are in
    // different disconnected trees, but this should not happen).
    while (ancestorStack.length > 1) {
      const top = ancestorStack[ancestorStack.length - 1]!;
      if (top.domNode.contains(currentNode)) {
        break;
      }

      // We are finished with 'top' (left its subtree).
      // At this point there is no chance for that node to be a parent of a
      // future visited node because its subtree has been fully traversed.
      ancestorStack.pop();

      // Post-Pruning:
      // If the node we just finished was invisible AND has no children (meaning
      // it has no visible content), we remove it. This mimics Blink's logic:
      // "Include hidden node ONLY if it has visible content".
      if (!top.isVisible && top.apcNode.childrenNodes.length === 0) {
        const parent = ancestorStack[ancestorStack.length - 1]!;
        const childrenOfParent = parent.apcNode.childrenNodes;
        // Remove strictly if it's the last child. It should be as the stack
        // follows a LIFO order and is walked pre-order, so the last
        // children corresponds to the node (`top`) that was just popped.
        if (childrenOfParent[childrenOfParent.length - 1] === top.apcNode) {
          childrenOfParent.pop();
        }
      }
    }

    if (ancestorStack.length === 0) {
      // End the walk if the stack was emptied which should never happen since
      // the minimum stack size is 1 (root node), but we add this guard for
      // documentation purposes.
      break;
    }

    // 2. Generate Content Node. Skip nodes that are too deep while keep
    // walking the tree since future nodes might be shallow enough.
    generateAndPushContentNode(
        currentNode, nonce, maxDepth, ancestorStack, interactiveNodeIds);

    currentNode = walker.nextNode();
  }

  // Final Cleanup: Pop remaining items from stack (except root) to apply
  // pruning logic.
  while (ancestorStack.length > 1) {
    const top = ancestorStack.pop()!;
    if (!top.isVisible && top.apcNode.childrenNodes.length === 0) {
      const parent = ancestorStack[ancestorStack.length - 1]!;
      const childrenOfParent = parent.apcNode.childrenNodes;
      childrenOfParent.pop();
    }
  }

  const pageInteractionInfo = extractPageInteractionInfo(document);

  return {
    rootNode: rootNode,
    pageInteractionInfo: pageInteractionInfo,
    frameData: extractFrameData(document),
    visibleBoundingBoxesForPasswordRedaction: [],
  };
}
