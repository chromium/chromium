// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HAS_BEEN_PASSWORD_SYMBOL} from '//components/autofill/ios/form_util/resources/fill_constants.js';
import {APC_NODE_DEPTH_COST, getRemoteFrameRemoteToken, NONCE_ATTR} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/common.js';
import {getNodeId, getOrCreateNodeId} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/dom_node_ids.js';
import {AxRole, FormControlType, PageContentAnchorRel, PageContentAnnotatedRole, PageContentAttributeType, PageContentClickabilityReason, PageContentInteractionDisabledReason, PageContentMediaType, PageContentRedactionDecision, PageContentTableRowType, PageContentTextSize} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/page_content_types.js';
import type {PageContent, PageContentAttributes, PageContentFormControlData, PageContentFormData, PageContentFrameData, PageContentFrameInteractionInfo, PageContentMediaData, PageContentNode, PageContentNodeInteractionInfo, PageContentPageInteractionInfo, PageContentScrollerInfo, PageContentTableData} from '//ios/chrome/browser/intelligence/proto_wrappers/resources/page_content_types.js';

// Set of DOM Node IDs that are considered interactive (focused, selection
// start/end). These nodes should be included in the APC tree even if they are
// generic containers.
type InteractiveNodeIds = Set<number>;

// Interface for elements that have a 'disabled' property.
interface HtmlElementWithDisabled extends HTMLElement {
  disabled: boolean;
}

// An HTMLInputElement that can be tracked with a Symbol property to indicate
// it has been a password field.
interface PasswordTrackedElement extends HTMLInputElement {
  [HAS_BEEN_PASSWORD_SYMBOL]?: boolean;
}

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
const TAG_CAPTION = 'CAPTION';

// Media tags.
const TAG_SVG = 'SVG';
const TAG_DESC = 'DESC';
const TAG_TITLE = 'TITLE';
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
const TAG_LABEL = 'LABEL';

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

// Dialog tags and attributes.
const TAG_DIALOG = 'DIALOG';
const ATTRIBUTE_OPEN_DIALOG = 'open';

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
  TAG_CAPTION,
  TAG_DESC,
  TAG_TITLE,
];

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

// Metadata schema constants.
const SCHEMA_ORG_IDENTIFIER = 'schema.org';
const SCHEMA_IS_ACCESSIBLE_FOR_FREE_KEY = 'isAccessibleForFree';
const SCHEMA_PART_TYPE_KEY = '@type';
const SCHEMA_PART_CSS_SELECTOR_KEY = 'cssSelector';
const SCHEMA_PART_WEB_PAGE_ELEMENT_TYPE = 'WebPageElement';
const SCHEMA_CONTEXT_KEY = '@context';
const SCHEMA_HAS_PART_KEY = 'hasPart';

// Regex used to sanitize JSON payloads before parsing.
const TRAILING_COMMA_REGEX = /,\s*([\]}])/g;
const NEWLINE_REGEX = /\n/g;

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

// Attribute keys.
const ATTR_KEY_ARIA_DISABLED = 'aria-disabled';
const ATTR_KEY_ARIA_HASPOPUP = 'aria-haspopup';
const ATTR_KEY_ARIA_EXPANDED = 'aria-expanded';
const ATTR_KEY_ROLE = 'role';
const ATTR_KEY_TABINDEX = 'tabindex';
const ATTR_KEY_AUTOCOMPLETE = 'autocomplete';
const ATTR_KEY_ONCLICK = 'onclick';
const ATTR_KEY_ONMOUSEDOWN = 'onmousedown';
const ATTR_KEY_ONMOUSEUP = 'onmouseup';
const ATTR_KEY_ONMOUSEOVER = 'onmouseover';
const ATTR_KEY_ONMOUSEENTER = 'onmouseenter';
const ATTR_KEY_ONKEYDOWN = 'onkeydown';
const ATTR_KEY_ONKEYUP = 'onkeyup';
const ATTR_KEY_ONKEYPRESS = 'onkeypress';

// Attribute and style values.
const ATTR_VALUE_TRUE = 'true';
const ATTR_VALUE_FALSE = 'false';
const ATTR_VALUE_CURSOR_NOT_ALLOWED = 'not-allowed';
const ATTR_VALUE_CURSOR_POINTER = 'pointer';
const ATTR_VALUE_ROLE_BUTTON = 'button';
const ATTR_VALUE_ROLE_LINK = 'link';
const ATTR_VALUE_ROLE_CHECKBOX = 'checkbox';
const ATTR_VALUE_ROLE_MENUITEM = 'menuitem';
const ATTR_VALUE_ROLE_MENUITEMCHECKBOX = 'menuitemcheckbox';
const ATTR_VALUE_ROLE_MENUITEMRADIO = 'menuitemradio';
const ATTR_VALUE_ROLE_OPTION = 'option';
const ATTR_VALUE_ROLE_RADIO = 'radio';
const ATTR_VALUE_ROLE_SWITCH = 'switch';
const ATTR_VALUE_ROLE_TAB = 'tab';
const ATTR_VALUE_ROLE_BANNER = 'banner';
const ATTR_VALUE_ROLE_NAVIGATION = 'navigation';
const ATTR_VALUE_ROLE_SEARCH = 'search';
const ATTR_VALUE_ROLE_MAIN = 'main';
const ATTR_VALUE_ROLE_ARTICLE = 'article';
const ATTR_VALUE_ROLE_REGION = 'region';
const ATTR_VALUE_ROLE_COMPLEMENTARY = 'complementary';
const ATTR_VALUE_ROLE_CONTENT_INFO = 'contentinfo';
const ATTR_VALUE_ROLE_NONE = 'none';

// Style values.
const ATTR_POSITION_FIXED = 'fixed';
const ATTR_POSITION_STICKY = 'sticky';
const ATTR_DISPLAY_NONE = 'none';
const ATTR_VISIBILITY_HIDDEN = 'hidden';
const ATTR_VISIBILITY_VISIBLE = 'visible';
const ATTR_TRANSFORM_UPPERCASE = 'uppercase';
const ATTR_TRANSFORM_LOWERCASE = 'lowercase';
const ATTR_TRANSFORM_CAPITALIZE = 'capitalize';
const ATTR_MASKING_NONE = 'none';
const ATTR_MASKING_CIRCLE = 'circle';
const ATTR_MASKING_SQUARE = 'square';
const ATTR_WHITESPACE_NORMAL = 'normal';
const ATTR_WHITESPACE_NOWRAP = 'nowrap';

// Set of AxRoles that imply interactivity.
const INTERACTIVE_AX_ROLES = new Set([
  AxRole.AX_ROLE_BUTTON,
  AxRole.AX_ROLE_LINK,
  AxRole.AX_ROLE_CHECK_BOX,
  AxRole.AX_ROLE_MENU_ITEM,
  AxRole.AX_ROLE_MENU_ITEM_CHECK_BOX,
  AxRole.AX_ROLE_MENU_ITEM_RADIO,
  AxRole.AX_ROLE_LIST_BOX_OPTION,
  AxRole.AX_ROLE_RADIO_BUTTON,
  AxRole.AX_ROLE_SWITCH,
  AxRole.AX_ROLE_TAB,
]);

const BASIC_CONTENT_ATTRIBUTES: PageContentAttributes = {
  attributeType: PageContentAttributeType.UNKNOWN,
  annotatedRoles: [],
  isAdRelated: false,
};

// Style values.
const STYLE_VALUE_OVERFLOW_AUTO = 'auto';
const STYLE_VALUE_OVERFLOW_SCROLL = 'scroll';
const STYLE_VALUE_CONTENT_VISIBILITY_HIDDEN = 'hidden';


// Type alias for accessing webkit-specific fullscreen document properties that
// are not part of the standard Document interface.
type WebkitDocument = Document&{webkitFullscreenElement?: Element};

// Math constants.
const SECOND_TO_MS_RATIO = 1000;

// ARIA Constants.
const ARIA_LABELLEDBY = 'aria-labelledby';
const ARIA_LABEL = 'aria-label';
// Regex used to split aria strings.
const SPACE_SEPARATOR = /\s+/;

/**
 * Returns true if page context IPC optimization is enabled.
 */
function isPageContextIPCOptimizationEnabled() {
  return (window as any).gCrWebPlaceholderPageContextIPCOptimization ?? false;
}

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

/**
 * Maps an ARIA role attribute value to its corresponding
 * PageContentAnnotatedRole.
 *
 * @param ariaRoleAttr The ARIA role attribute value to map.
 * @return The corresponding PageContentAnnotatedRole, or undefined if no
 *     mapping exists.
 */
function getAnnotatedRoleForAriaRole(ariaRoleAttr: string):
    PageContentAnnotatedRole|undefined {
  // Split the role string by one or more whitespace characters.
  const roles = ariaRoleAttr.trim().split(SPACE_SEPARATOR);

  // Since multiple space-separated fallback roles are allowed, return the
  // first valid match.
  for (const role of roles) {
    switch (role.toLowerCase()) {
      case ATTR_VALUE_ROLE_BANNER:
        return PageContentAnnotatedRole.HEADER;
      case ATTR_VALUE_ROLE_NAVIGATION:
        return PageContentAnnotatedRole.NAV;
      case ATTR_VALUE_ROLE_SEARCH:
        return PageContentAnnotatedRole.SEARCH;
      case ATTR_VALUE_ROLE_MAIN:
        return PageContentAnnotatedRole.MAIN;
      case ATTR_VALUE_ROLE_ARTICLE:
        return PageContentAnnotatedRole.ARTICLE;
      case ATTR_VALUE_ROLE_REGION:
        return PageContentAnnotatedRole.SECTION;
      case ATTR_VALUE_ROLE_COMPLEMENTARY:
        return PageContentAnnotatedRole.ASIDE;
      case ATTR_VALUE_ROLE_CONTENT_INFO:
        return PageContentAnnotatedRole.FOOTER;
      default:
        continue;
    }
  }
  return undefined;
}

/**
 * Maps an ARIA role attribute value to its corresponding AxRole enum.
 *
 * @param roleString The raw ARIA role string to map.
 * @return The corresponding AxRole, or AX_ROLE_UNKNOWN if no mapping exists.
 */
function getAXRoleForAriaRole(roleString: string): AxRole {
  const roles = roleString.trim().split(SPACE_SEPARATOR);

  for (const role of roles) {
    switch (role.toLowerCase()) {
      case ATTR_VALUE_ROLE_BANNER:
        return AxRole.AX_ROLE_BANNER;
      case ATTR_VALUE_ROLE_BUTTON:
        return AxRole.AX_ROLE_BUTTON;
      case ATTR_VALUE_ROLE_CHECKBOX:
        return AxRole.AX_ROLE_CHECK_BOX;
      case ATTR_VALUE_ROLE_LINK:
        return AxRole.AX_ROLE_LINK;
      case ATTR_VALUE_ROLE_MENUITEM:
        return AxRole.AX_ROLE_MENU_ITEM;
      case ATTR_VALUE_ROLE_MENUITEMCHECKBOX:
        return AxRole.AX_ROLE_MENU_ITEM_CHECK_BOX;
      case ATTR_VALUE_ROLE_MENUITEMRADIO:
        return AxRole.AX_ROLE_MENU_ITEM_RADIO;
      case ATTR_VALUE_ROLE_OPTION:
        return AxRole.AX_ROLE_LIST_BOX_OPTION;
      case ATTR_VALUE_ROLE_RADIO:
        return AxRole.AX_ROLE_RADIO_BUTTON;
      case ATTR_VALUE_ROLE_SWITCH:
        return AxRole.AX_ROLE_SWITCH;
      case ATTR_VALUE_ROLE_TAB:
        return AxRole.AX_ROLE_TAB;
      case ATTR_VALUE_ROLE_NAVIGATION:
        return AxRole.AX_ROLE_NAVIGATION;
      case ATTR_VALUE_ROLE_SEARCH:
        return AxRole.AX_ROLE_SEARCH;
      case ATTR_VALUE_ROLE_MAIN:
        return AxRole.AX_ROLE_MAIN;
      case ATTR_VALUE_ROLE_ARTICLE:
        return AxRole.AX_ROLE_ARTICLE;
      case ATTR_VALUE_ROLE_REGION:
        return AxRole.AX_ROLE_REGION;
      case ATTR_VALUE_ROLE_COMPLEMENTARY:
        return AxRole.AX_ROLE_COMPLEMENTARY;
      case ATTR_VALUE_ROLE_CONTENT_INFO:
        return AxRole.AX_ROLE_CONTENT_INFO;
      case ATTR_VALUE_ROLE_NONE:
        return AxRole.AX_ROLE_NONE;
      default:
        continue;
    }
  }
  return AxRole.AX_ROLE_UNKNOWN;
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
  const tagName = getStandardTagName(element);

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
 * Determines if an element is rendered in the top layer.
 *
 * @param element The element to check.
 * @return True if the element is rendered in the top layer, false otherwise.
 */
function isRenderedInTopLayer(element: HTMLElement): boolean {
  // 1. Open popovers. Use CSS.supports for browsers not supporting
  // :popover-open.
  if (CSS.supports('selector(:popover-open)') &&
      element.matches(':popover-open')) {
    return true;
  }

  // 2. Dialogs opened as modals.
  if (CSS.supports('selector(:modal)')) {
    if (element.matches(':modal')) {
      return true;
    }
  } else {
    // Fallback: check if it's an open dialog. Note: Without `:modal` support,
    // we cannot distinguish between `dialog.show()` (normal document flow) and
    // `dialog.showModal()` (top layer). This is a best-effort approximation.
    if (getStandardTagName(element) === TAG_DIALOG &&
        element.hasAttribute(ATTRIBUTE_OPEN_DIALOG)) {
      return true;
    }
  }

  // 3. Fullscreen element.
  const doc = element.ownerDocument;
  return doc &&
      (doc.fullscreenElement === element ||
       (doc as WebkitDocument).webkitFullscreenElement === element);
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
 * @param interactiveNodeIds The set of interactive node IDs.
 * @param interactionInfo The pre-calculated interaction info for the element.
 * @param annotatedRoles The annotated roles for the element.
 * @return True if the element is a generic container, false otherwise.
 */
function isGenericContainer(
    element: HTMLElement, interactiveNodeIds: InteractiveNodeIds,
    interactionInfo: PageContentNodeInteractionInfo|undefined,
    annotatedRoles: PageContentAnnotatedRole[],
    labelForDOMNodeID?: number): boolean {
  // If the element is a label with a valid associated node ID, it is
  // considered a generic container.
  if (labelForDOMNodeID !== undefined) {
    return true;
  }

  // Check if the element is an interactive node.
  const nodeId = getNodeId(element);
  const tagName = getStandardTagName(element);
  if (nodeId !== null && interactiveNodeIds.has(nodeId)) {
    return true;
  }

  // Elements with annotated roles are considered generic containers.
  if (annotatedRoles.length > 0) {
    return true;
  }

  // A <figure> element is a semantic container for self-contained content, like
  // images or diagrams, making it a generic container.
  if (tagName === TAG_FIGURE) {
    return true;
  }

  // Elements with fixed or sticky positioning are removed from the normal flow
  // and often act as containers for UI elements like headers or sidebars.
  const windowObj = element.ownerDocument?.defaultView;
  if (!windowObj) {
    return false;
  }

  // Treat as container if we extracted interaction info (e.g. it's scrollable).
  if (interactionInfo) {
    return true;
  }

  const style = windowObj.getComputedStyle(element);
  const position = style.position;
  if (position === ATTR_POSITION_FIXED || position === ATTR_POSITION_STICKY) {
    return true;
  }

  if (isRenderedInTopLayer(element)) {
    return true;
  }

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
 * Extracts scroller information from an element if it is scrollable.
 *
 * @param element The element to check.
 * @param style The computed style of the element.
 * @return The PageContentScrollerInfo or undefined if not scrollable.
 */
function getScrollerInfo(element: HTMLElement, style: CSSStyleDeclaration):
    PageContentScrollerInfo|undefined {
  const overflowX = style.overflowX;
  const overflowY = style.overflowY;

  const isScrollableX = overflowX === STYLE_VALUE_OVERFLOW_SCROLL ||
      overflowX === STYLE_VALUE_OVERFLOW_AUTO;
  const isScrollableY = overflowY === STYLE_VALUE_OVERFLOW_SCROLL ||
      overflowY === STYLE_VALUE_OVERFLOW_AUTO;

  // We consider it a scroller if configured to scroll AND there is overflow
  // (scrollHeight > clientHeight), OR if 'scroll' is forced.
  // Blink's logic `ScrollsOverflow` checks overflow style.
  if (!isScrollableX && !isScrollableY) {
    return undefined;
  }

  // TODO(crbug.com/480945289): Remove this when page context IPC optimization
  // is enabled.
  if (isPageContextIPCOptimizationEnabled()) {
    // Make sure to call element.clientWidth before element.scrollWidth.
    // This will guide the layout engine to perform the shallow layout first
    // and then the deep layout calculation.
    const visibleArea = {
      x: element.scrollLeft,
      y: element.scrollTop,
      width: element.clientWidth,
      height: element.clientHeight,
      top: element.scrollTop,
      right: element.scrollLeft + element.clientWidth,
      bottom: element.scrollTop + element.clientHeight,
      left: element.scrollLeft,
    };

    // Populate bounds.
    // Scrolling bounds = whole content size.
    const scrollingBounds = {
      width: element.scrollWidth,
      height: element.scrollHeight,
    };

    return {
      scrollingBounds,
      visibleArea,
      userScrollableHorizontal:
          isScrollableX && (element.scrollWidth > element.clientWidth),
      userScrollableVertical:
          isScrollableY && (element.scrollHeight > element.clientHeight),
    };
  } else {
    // Populate bounds.
    // Scrolling bounds = whole content size.
    const scrollingBounds = {
      width: element.scrollWidth,
      height: element.scrollHeight,
    };

    const visibleArea = {
      x: element.scrollLeft,
      y: element.scrollTop,
      width: element.clientWidth,
      height: element.clientHeight,
      top: element.scrollTop,
      right: element.scrollLeft + element.clientWidth,
      bottom: element.scrollTop + element.clientHeight,
      left: element.scrollLeft,
    };

    return {
      scrollingBounds,
      visibleArea,
      userScrollableHorizontal:
          isScrollableX && (element.scrollWidth > element.clientWidth),
      userScrollableVertical:
          isScrollableY && (element.scrollHeight > element.clientHeight),
    };
  }
}

/**
 * Computes the interaction info for the element.
 *
 * @param element The element to process.
 * @param actionableMode Whether to extract actionable interaction info.
 * @param hasCanvas Whether there is a canvas element on the page.
 * @return The populated PageContentNodeInteractionInfo or undefined if none.
 */
function getNodeInteractionInfo(
    element: HTMLElement, actionableMode: boolean,
    hasCanvas: boolean): PageContentNodeInteractionInfo|undefined {
  const interactionInfo: PageContentNodeInteractionInfo = {
    clickabilityReasons: [],
    isDisabled: false,
    interactionDisabledReasons: [],
    isFocusable: false,
  };

  // If we are not in actionable mode and there is no canvas, we can skip
  // everything. This assumes that scroller info is only used to compute the
  // canvas heavy heuristic.
  if (!actionableMode && !hasCanvas) {
    return undefined;
  }

  const style = window.getComputedStyle(element);

  // Scroller Info
  const scrollerInfo = getScrollerInfo(element, style);
  if (scrollerInfo) {
    interactionInfo.scrollerInfo = scrollerInfo;
  }

  // Just return the scroller info when not in actionable mode.
  if (!actionableMode) {
    return scrollerInfo ? interactionInfo : undefined;
  }

  // TODO(crbug.com/486460634): Double check that everything is there to support
  // actionable mode.

  const interactionDisabledReasons = interactionInfo.interactionDisabledReasons;

  // Disabled Check.
  let isDisabled = false;
  // Check 'disabled' property for form elements.
  // Note: we cast to any because the 'disabled' property is not on HTMLElement
  // but specific subclasses like HTMLInputElement, HTMLButtonElement, etc.
  if ('disabled' in element && (element as HtmlElementWithDisabled).disabled) {
    interactionDisabledReasons.push(
        PageContentInteractionDisabledReason.DISABLED);
    isDisabled = true;
  }
  // Check aria-disabled.
  if (element.getAttribute(ATTR_KEY_ARIA_DISABLED) === ATTR_VALUE_TRUE) {
    interactionDisabledReasons.push(
        PageContentInteractionDisabledReason.ARIA_DISABLED);
    isDisabled = true;
  }
  // Check cursor: not-allowed.
  if (style.cursor === ATTR_VALUE_CURSOR_NOT_ALLOWED) {
    interactionDisabledReasons.push(
        PageContentInteractionDisabledReason.CURSOR_NOT_ALLOWED);
  }
  interactionInfo.isDisabled = isDisabled;

  const clickabilityReasons = interactionInfo.clickabilityReasons;

  const tagName = getStandardTagName(element);
  // Form Controls.
  if ([TAG_BUTTON, TAG_INPUT, TAG_SELECT, TAG_TEXTAREA].includes(tagName)) {
    clickabilityReasons.push(PageContentClickabilityReason.CLICKABLE_CONTROL);
  }

  // Event handlers.
  // Unfortunately we can't easily detect event listeners added via
  // addEventListener. However we can detect inline handler attributes.
  if (element.hasAttribute(ATTR_KEY_ONCLICK)) {
    clickabilityReasons.push(PageContentClickabilityReason.CLICK_EVENTS);
  }
  if (element.hasAttribute(ATTR_KEY_ONMOUSEDOWN) ||
      element.hasAttribute(ATTR_KEY_ONMOUSEUP)) {
    clickabilityReasons.push(PageContentClickabilityReason.MOUSE_CLICK);
  }
  if (element.hasAttribute(ATTR_KEY_ONMOUSEOVER) ||
      element.hasAttribute(ATTR_KEY_ONMOUSEENTER)) {
    clickabilityReasons.push(PageContentClickabilityReason.MOUSE_HOVER);
  }
  if (element.hasAttribute(ATTR_KEY_ONKEYDOWN) ||
      element.hasAttribute(ATTR_KEY_ONKEYUP) ||
      element.hasAttribute(ATTR_KEY_ONKEYPRESS)) {
    clickabilityReasons.push(PageContentClickabilityReason.KEY_EVENTS);
  }

  // Pointer Cursor.
  if (style.cursor === ATTR_VALUE_CURSOR_POINTER) {
    clickabilityReasons.push(PageContentClickabilityReason.CURSOR_POINTER);
  }

  // Editable.
  if (element.isContentEditable ||
      tagName === TAG_TEXTAREA ||
      (tagName === TAG_INPUT &&
       ![CHECKBOX_TYPE, RADIO_TYPE, RANGE_TYPE, COLOR_TYPE, FILE_TYPE,
         IMAGE_TYPE, SUBMIT_TYPE, RESET_TYPE, BUTTON_TYPE]
            .includes((element as HTMLInputElement).type))) {
    clickabilityReasons.push(PageContentClickabilityReason.EDITABLE);
  }

  // Aria Role that imply interactivity.
  const role = element.getAttribute(ATTR_KEY_ROLE);
  if (role) {
    const axRole = getAXRoleForAriaRole(role);
    if (INTERACTIVE_AX_ROLES.has(axRole)) {
      clickabilityReasons.push(PageContentClickabilityReason.ARIA_ROLE);
    }
  }

  // Aria Properties.
  if (element.hasAttribute(ATTR_KEY_ARIA_HASPOPUP)) {
    clickabilityReasons.push(PageContentClickabilityReason.ARIA_HAS_POPUP);
  }

  const ariaExpanded = element.getAttribute(ATTR_KEY_ARIA_EXPANDED);
  if (ariaExpanded === ATTR_VALUE_TRUE) {
    clickabilityReasons.push(PageContentClickabilityReason.ARIA_EXPANDED_TRUE);
  } else if (ariaExpanded === ATTR_VALUE_FALSE) {
    clickabilityReasons.push(PageContentClickabilityReason.ARIA_EXPANDED_FALSE);
  }

  // Tab Index.
  if (element.hasAttribute(ATTR_KEY_TABINDEX)) {
    clickabilityReasons.push(PageContentClickabilityReason.TAB_INDEX);
  }

  // Autocomplete.
  if (element.hasAttribute(ATTR_KEY_AUTOCOMPLETE)) {
    clickabilityReasons.push(PageContentClickabilityReason.AUTOCOMPLETE);
  }

  // Focusable.
  // A rough check: tabIndex >= 0 (e.g. implicitly focusable or explicit >= 0)
  // OR the element has a tabindex attribute (making it focusable efficiently if
  // -1). This avoids marking every single element as focusable since existing
  // HTMLElement.tabIndex defaults to -1 for non-focusable elements.
  // We also check for contenteditable which makes elements focusable even
  // without tabIndex.
  if (!isDisabled && style.visibility === ATTR_VISIBILITY_VISIBLE &&
      (element.tabIndex >= 0 || element.hasAttribute(ATTR_KEY_TABINDEX) ||
       element.isContentEditable)) {
    interactionInfo.isFocusable = true;
  }

  // Only assign if we found something relevant.
  if (interactionInfo.scrollerInfo || clickabilityReasons.length > 0 ||
      interactionInfo.interactionDisabledReasons.length > 0 ||
      interactionInfo.isFocusable) {
    return interactionInfo;
  }
  return undefined;
}

/**
 * Extracts media data from the document.
 *
 * @param document The document to extract data from.
 * @return The populated PageContentMediaData or undefined if no relevant media
 *     found.
 */
function extractMediaData(document: Document): PageContentMediaData|undefined {
  const mediaElements = Array.from(document.querySelectorAll('video, audio')) as
      HTMLMediaElement[];

  if (mediaElements.length === 0) {
    return undefined;
  }

  // Heuristic: Prefer the first playing media, otherwise the first one with a
  // valid duration.
  // TODO(crbug.com/489666605): Improve selection heuristic to match Blink's
  // WebMediaSessionManager logic if possible.
  let selectedMedia = mediaElements.find(m => !m.paused && !m.ended);
  if (!selectedMedia) {
    selectedMedia =
        mediaElements.find(m => !isNaN(m.duration) && m.duration > 0);
  }

  if (!selectedMedia) {
    return undefined;
  }

  const durationMilliseconds =
      Math.floor(selectedMedia.duration * SECOND_TO_MS_RATIO);

  // Duration is required for proto and must be a valid finite number.
  if (!Number.isFinite(durationMilliseconds)) {
    return undefined;
  }

  const tagName = getStandardTagName(selectedMedia);
  let mediaDataType = PageContentMediaType.MEDIA_DATA_TYPE_UNKNOWN;
  if (tagName === TAG_VIDEO) {
    mediaDataType = PageContentMediaType.MEDIA_DATA_TYPE_VIDEO;
  } else if (tagName === TAG_AUDIO) {
    mediaDataType = PageContentMediaType.MEDIA_DATA_TYPE_AUDIO;
  }

  return {
    mediaDataType,
    durationMilliseconds,
    currentPositionMilliseconds:
        Math.floor(selectedMedia.currentTime * SECOND_TO_MS_RATIO),
    isPlaying: !selectedMedia.paused && !selectedMedia.ended,
  };
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

// Context for paid content extraction.
interface PaidContentExtractionContext {
  /** Whether the paid content extraction is enabled. */
  extractPaidContent: boolean;
  /** Whether to attempt to fix malformed paid content JSON. */
  attemptPaidContentJsonFixing: boolean;
  /** Whether the page contains paid content. */
  containsPaidContent: boolean;
  /** The set of DOM nodes verified as paid content. */
  paidNodes: Set<Node>;
}

/**
 * Parses the `hasPart` field of a schema.org object and populates the
 * `paidNodes` set with elements matching the `cssSelector` of `WebPageElement`s
 * that are not accessible for free.
 *
 * @param document The document to query.
 * @param hasPart The `hasPart` value from the JSON-LD object.
 * @param paidNodes The set to populate with paid content nodes.
 */
function extractPaidNodesFromHasPart(
    document: Document, hasPart: unknown, paidNodes: Set<Node>) {
  if (!hasPart) {
    return;
  }
  const hasPartsArray = Array.isArray(hasPart) ? hasPart : [hasPart];
  for (const part of hasPartsArray) {
    if (typeof part !== 'object' || part === null) {
      continue;
    }

    const partRecord = part as Record<string, unknown>;
    if (partRecord[SCHEMA_PART_TYPE_KEY] !==
        SCHEMA_PART_WEB_PAGE_ELEMENT_TYPE) {
      continue;
    }

    const partIsFree = partRecord[SCHEMA_IS_ACCESSIBLE_FOR_FREE_KEY];
    // Blink's ObjectValuePresentAndFalse explicitly checks boolean false,
    // or strings "false" and "False". We mirror that exact behavior here
    // rather than fully normalizing to lowercase.
    if (partIsFree !== false && partIsFree !== 'false' &&
        partIsFree !== 'False') {
      continue;
    }

    const selector = partRecord[SCHEMA_PART_CSS_SELECTOR_KEY];
    if (typeof selector !== 'string') {
      continue;
    }

    try {
      // document.querySelectorAll throws a DOMException if the selector is
      // invalid. We catch it to avoid crashing the extraction process.
      const elements = document.querySelectorAll(selector);
      for (const el of Array.from(elements)) {
        paidNodes.add(el);
      }
    } catch (e) {
      // Ignore invalid css selectors.
    }
  }
}

/**
 * Helper function to parse JSON, with fallbacks for common syntax errors.
 * Mirrors Blink's ParsePaidContentJSON logic.
 *
 * @param jsonString The raw JSON string.
 * @param attemptPaidContentJsonFixing Whether to attempt fixing malformed JSON.
 * @return The parsed JSON object or null if parsing completely fails.
 */
function parsePaidContentJson(
    jsonString: string, attemptPaidContentJsonFixing: boolean): any|null {
  try {
    // Fast path: Try parsing the standard JSON first.
    // This handles well-formed JSON without regex performance hits or string
    // corruption.
    return JSON.parse(jsonString);
  } catch (e) {
    // Ignore error, proceed to slow path if enabled.
  }

  if (!attemptPaidContentJsonFixing) {
    return null;
  }

  // Slow path: Fallback for malformed JSON seen in the wild.
  // The JSON provided by some websites has unescaped newlines or trailing
  // commas. This regex is a best-effort recovery.
  let sanitizedText = jsonString.replace(NEWLINE_REGEX, ' ');
  sanitizedText = sanitizedText.replace(TRAILING_COMMA_REGEX, '$1');
  try {
    return JSON.parse(sanitizedText);
  } catch (fallbackError) {
    // Both fast path and slow path failed.
    return null;
  }
}

/**
 * Checks if the document contains paid content by inspecting ld+json metadata
 * or microdata fallbacks and builds a set of exact matched DOM nodes.
 *
 * @param document The document to check.
 * @return An object containing the global boolean flag and a Set of specific
 *         DOM nodes annotated as paid content.
 */
function extractContainsPaidContent(
    document: Document, extractPaidContent: boolean,
    attemptPaidContentJsonFixing: boolean): PaidContentExtractionContext {
  const extractionContext: PaidContentExtractionContext = {
    extractPaidContent,
    attemptPaidContentJsonFixing,
    containsPaidContent: false,
    paidNodes: new Set<Node>(),
  };

  if (!extractPaidContent) {
    // Return an unfilled context if paid content extraction is not enabled,
    // meaning paid content will not be taken into account.
    return extractionContext;
  }

  const head = document.head;
  if (!head) {
    return extractionContext;
  }

  const scripts = head.querySelectorAll('script[type="application/ld+json"]');
  for (const script of Array.from(scripts)) {
    if (!script.textContent) {
      continue;
    }

    const obj =
        parsePaidContentJson(script.textContent, attemptPaidContentJsonFixing);

    if (typeof obj !== 'object' || obj === null || Array.isArray(obj)) {
      // Skip any JSON value that isn't a dictionary object.
      continue;
    }

    // Check for "schema.org" in "@context".
    const jsonContext = obj[SCHEMA_CONTEXT_KEY];
    if (typeof jsonContext !== 'string' ||
        !jsonContext.includes(SCHEMA_ORG_IDENTIFIER)) {
      continue;
    }

    // Check for isAccessibleForFree=false or "false" or "False".
    const isAccessibleForFree = obj[SCHEMA_IS_ACCESSIBLE_FOR_FREE_KEY];
    // Blink's ObjectValuePresentAndFalse explicitly checks boolean false,
    // or strings "false" and "False". We mirror that exact behavior here
    // rather than fully normalizing to lowercase.
    if (isAccessibleForFree !== false && isAccessibleForFree !== 'false' &&
        isAccessibleForFree !== 'False') {
      continue;
    }

    extractionContext.containsPaidContent = true;

    // Check for hasPart with cssSelector.
    extractPaidNodesFromHasPart(
        document, obj[SCHEMA_HAS_PART_KEY], extractionContext.paidNodes);

    if (extractionContext.paidNodes.size > 0) {
      // Nodes mapped explicitly via cssSelector, no need to fallback.
      return extractionContext;
    }

    // We successfully parsed the JSON but found no specific CSS
    // selectors. By Blink's rules, we exit tracking as true but
    // fallthrough the inner loop to potentially trigger microdata
    // fallback for node linking.
    break;
  }

  // Fallback: If no valid cssSelector mappings were found, query the document
  // for <meta itemprop="isAccessibleForFree" content="false"> metadata.
  if (extractionContext.paidNodes.size === 0) {
    const paidMetaTags = document.querySelectorAll(`meta[itemprop="${
        SCHEMA_IS_ACCESSIBLE_FOR_FREE_KEY}"][content="false" i]`);
    if (paidMetaTags.length > 0) {
      extractionContext.containsPaidContent = true;
      for (const meta of Array.from(paidMetaTags)) {
        if (meta.parentElement) {
          extractionContext.paidNodes.add(meta.parentElement);
        }
      }
    }
  }

  return extractionContext;
}

// TODO(crbug.com/468854910): Add missing fields for PageContentFrameData:
// popup (if possible).
/**
 * Extracts data about the frame/document.
 *
 * @param document The document to extract data from.
 * @param paidContentResult The pre-extracted paid content data.
 * @return The populated PageContentFrameData.
 */
function extractFrameData(
    document: Document,
    paidContentContext: PaidContentExtractionContext): PageContentFrameData {
  const frameData: PageContentFrameData = {
    frameInteractionInfo: {},
    metaData: [],
    title: document.title || '',
    sourceUrl: document.URL,
  };

  frameData.containsPaidContent = paidContentContext.containsPaidContent;

  frameData.frameInteractionInfo = extractFrameInteractionInfo(document);
  frameData.mediaData = extractMediaData(document);

  return frameData;
}

/**
 * Parses a CSS color string and returns it as a packed RGBA uint32.
 * Supported formats: rgb(r, g, b), rgba(r, g, b, a).
 * Returns null if the color string is invalid.
 *
 * @param colorString The CSS color string to parse.
 * @return The packed color value ((r << 24) | (g << 16) | (b << 8) | a).
 */
function parseCssColor(colorString: string): number | undefined {
  if (!colorString) {
    return undefined;
  }

  // Handle rgb(r, g, b).
  const rgbMatch = colorString.match(/^rgb\(\s*(\d+),\s*(\d+),\s*(\d+)\s*\)$/);
  if (rgbMatch) {
    const r = parseInt(rgbMatch[1]!, 10);
    const g = parseInt(rgbMatch[2]!, 10);
    const b = parseInt(rgbMatch[3]!, 10);
    // Fully opaque alpha = 255
    return (((r & 0xFF) << 24) | ((g & 0xFF) << 16) | ((b & 0xFF) << 8) | 0xFF) >>> 0;
  }

  // Handle rgba(r, g, b, a).
  const rgbaMatch =
    colorString.match(/^rgba\(\s*(\d+),\s*(\d+),\s*(\d+),\s*([\d.]+)\s*\)$/);
  if (rgbaMatch) {
    const r = parseInt(rgbaMatch[1]!, 10);
    const g = parseInt(rgbaMatch[2]!, 10);
    const b = parseInt(rgbaMatch[3]!, 10);
    const a = Math.round(parseFloat(rgbaMatch[4]!) * 255);
    return (((r & 0xFF) << 24) | ((g & 0xFF) << 16) | ((b & 0xFF) << 8) | (a & 0xFF)) >>> 0;
  }

  return undefined;
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
    if (transform === ATTR_TRANSFORM_UPPERCASE) {
      maskedText = maskedText.toUpperCase();
    } else if (transform === ATTR_TRANSFORM_LOWERCASE) {
      maskedText = maskedText.toLowerCase();
    } else if (transform === ATTR_TRANSFORM_CAPITALIZE) {
      maskedText = maskedText.replace(
          TEXT_TRANSFORM_CAPITALIZE_REGEX, (char) => char.toUpperCase());
    }
  }

  // 2. Text Masking (-webkit-text-security).
  // This property is not in the standard CSSStyleDeclaration type.
  const masking = (style as any).webkitTextSecurity;
  if (masking && masking !== ATTR_MASKING_NONE) {
    let maskChar = TEXT_MASKING_CHAR_DISC;
    if (masking === ATTR_MASKING_CIRCLE) {
      maskChar = TEXT_MASKING_CHAR_CIRCLE;
    } else if (masking === ATTR_MASKING_SQUARE) {
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
    if ([ATTR_WHITESPACE_NORMAL, ATTR_WHITESPACE_NOWRAP].includes(whiteSpace)) {
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
  const color = parseCssColor(style.color);

  return {
    attributeType: PageContentAttributeType.TEXT,
    annotatedRoles: [],
    isAdRelated: false,
    textInfo: {
      textContent: maskedText,
      textStyle: {
        textSize: textSize,
        hasEmphasis,
        color,
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
 * @param actionableMode Whether to extract actionable interaction info.
 * @return The populated PageContentNode for the iframe.
 */
function getContentForIframeNode(
    iframeElement: HTMLIFrameElement, nonce: string, depth: number,
    maxDepth: number, actionableMode: boolean,
    paidContentContext: PaidContentExtractionContext): PageContentNode|null {
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
          contentDoc, nonce, depth + APC_NODE_DEPTH_COST, maxDepth,
          actionableMode, paidContentContext.extractPaidContent,
          paidContentContext.attemptPaidContentJsonFixing);
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
 * Populates the `label` attribute by extracting the accessible name
 * from `aria-labelledby` or `aria-label`, following the W3C spec where
 * `aria-labelledby` takes precedence.
 *
 * TODO(crbug.com/494224739): Remove the following note when desktop's
 * implementation is updated to follow the W3C spec.
 *
 * Note: This differs from desktop's implementation which concatenates
 * these two fields if both are present.
 *
 * @param element - The DOM element to extract labels from.
 * @returns The resulting string or undefined if none found.
 */
function getAriaLabel(element: HTMLElement): string | undefined {
  const accumulatedTexts: string[] = [];

  // Process aria-labelledby.
  const labelledBy = element.getAttribute(ARIA_LABELLEDBY)?.trim();
  if (labelledBy) {
    const ids = labelledBy.split(SPACE_SEPARATOR);
    // This will only work if the labelElement and the element share the same
    // root. It won't work if the two elements are in different shadow DOMs.
    // This follows the web standard.
    const rootNode = element.getRootNode() as Document | ShadowRoot;

    for (const id of ids) {
      if (!id) {
        continue;
      }

      const labelElement = rootNode.getElementById?.(id);
      // We use textContent instead of innerText
      // because elements referenced by aria-labelledby may not be visible.
      const textContent = labelElement?.textContent;
      if (textContent && textContent.trim().length > 0) {
        accumulatedTexts.push(textContent);
      }
    }
  }

  // Process aria-label if aria-labelledby is not present.
  if (accumulatedTexts.length === 0) {
    const ariaLabel = element.getAttribute(ARIA_LABEL);
    if (ariaLabel && ariaLabel.trim().length > 0) {
      accumulatedTexts.push(ariaLabel);
    }
  }

  if (accumulatedTexts.length > 0) {
    return accumulatedTexts.join(' ');
  }
  return undefined;
}

/**
 * Checks if a code point is a security mask character (e.g. bullet, asterisk).
 *
 * @param codePoint The Unicode code point to check.
 * @return True if the code point is a security mask character.
 */
function isSecurityMaskCharacter(codePoint: number): boolean {
  switch (codePoint) {
    // Standard Asterisks & Stars.
    case 0x002A:  // '*'
    case 0x2731:  // Heavy Asterisk (✱)
    case 0x2732:  // Open Centre Asterisk (✲)
    case 0x2733:  // Eight Spoked Asterisk (✳)
    case 0xFF0A:  // Fullwidth Asterisk (＊)

    // Standard Bullets & Circles.
    case 0x2022:  // Bullet (•)
    case 0x25CF:  // Black Circle (●)
    case 0x25CB:  // White Circle (○)
    case 0x25EF:  // Large Circle (◯)
    case 0x26AB:  // Medium Black Circle (⚫)
    case 0x2B24:  // Black Large Circle (⬤)
    case 0x25E6:  // White Bullet (◦)
    case 0x25C9:  // Fisheye (◉)

    // Dots & Mathematical Operators.
    case 0x00B7:  // Middle Dot (·)
    case 0x2219:  // Bullet Operator (∙)
    case 0x22C5:  // Dot Operator (⋅)
    case 0x2802:  // Braille Dot-2 (⠂)
    case 0x2812:  // Braille Dots-2-5 (⠒)
    case 0x2836:  // Braille Dots-2-3-5-6 (⠶)

    // Squares, Blocks & Diamonds.
    case 0x25A0:  // Black Square (■)
    case 0x25A1:  // White Square (□)
    case 0x25AA:  // Black Small Square (▪)
    case 0x25AB:  // White Small Square (▫)
    case 0x25AE:  // Black Vertical Rectangle (▮)
    case 0x2588:  // Full Block (█)
    case 0x2589:  // Left Seven Eighths Block (▉)
    case 0x25C6:  // Black Diamond (◆)
    case 0x25C7:  // White Diamond (◇)
      return true;

    default:
      return false;
  }
}

/**
 * Checks if a code point is a whitespace character (Space, Tab, LF, CR).
 *
 * @param codePoint The Unicode code point to check.
 * @return True if the code point is a whitespace character.
 */
function isJSWhitespace(codePoint: number): boolean {
  switch (codePoint) {
    case 0x0020:  // Space
    case 0x0009:  // Horizontal tab
    case 0x000A:  // Line feed
    case 0x000D:  // Carriage return
      return true;
    default:
      return false;
  }
}

/**
 * Checks if the field's value looks like a custom password field masked by JS.
 * This heuristic detects values that are mostly composed of mask characters,
 * potentially with the last character visible (as is common on mobile).
 *
 * @param fieldValue The value to check.
 * @return True if the value is likely a custom password.
 */
function isLikelyJSCustomPasswordField(fieldValue: string): boolean {
  // Use a while loop to correctly iterate over Unicode code points without
  // allocating a new array, handling surrogate pairs as single characters.
  let i = 0;
  let codePointCount = 0;
  while (i < fieldValue.length) {
    const codePoint = fieldValue.codePointAt(i)!;
    codePointCount++;

    if (isJSWhitespace(codePoint)) {
      // Passwords generally do not contain whitespace (it doesn't mean they
      // can't but this is a generalization to make a best guess on the
      // purpose of the `fieldValue`).
      return false;
    }

    const isMask = isSecurityMaskCharacter(codePoint);
    const charLen = codePoint > 0xFFFF ? 2 : 1;
    const isLast = (i + charLen >= fieldValue.length);

    if (isMask) {
      i += charLen;
      continue;
    }

    // Visible characters are only allowed at the very end (to support the
    // common mobile pattern where the last typed character is briefly visible).
    if (!isLast) {
      return false;
    }

    i += charLen;
  }

  // All characters look like password characters and we have at least 2.
  return codePointCount >= 2;
}

/**
 * Checks if the element is a custom password field (e.g. using CSS
 * text-security or JS masking).
 */
function isCustomPassword(element: Element): boolean {
  if (element.tagName === TAG_INPUT || element.tagName === TAG_TEXTAREA) {
    const value = (element as HTMLInputElement | HTMLTextAreaElement).value;
    if (value && isLikelyJSCustomPasswordField(value)) {
      return true;
    }
  }

  const windowObj = element.ownerDocument?.defaultView || window;
  const style = windowObj.getComputedStyle(element);
  const textSecurity = style.getPropertyValue('-webkit-text-security');
  return !!textSecurity && textSecurity !== 'none';
}



/**
 * Checks if the element is a password field (standard or custom).
 *
 * @param domNode The DOM element to process.
 * @param tagName The tag name of the element.
 * @return True if the element is a password field.
 */
function isPasswordField(domNode: HTMLElement, tagName: string): boolean {
  if (tagName === TAG_INPUT &&
      ((domNode as PasswordTrackedElement)[HAS_BEEN_PASSWORD_SYMBOL] ||
       (domNode as HTMLInputElement).type === PASSWORD_TYPE)) {
    // A plain password input.
    return true;
  }

  if (tagName === TAG_INPUT || tagName === TAG_TEXTAREA) {
    // Check for custom password fields (CSS or JS masked).
    return isCustomPassword(domNode);
  }
  return false;
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
    redactionDecision: PageContentRedactionDecision.NO_REDACTION_NECESSARY,
  };

  const name = (domNode as HTMLInputElement).name;
  if (name !== undefined && name !== '') {
    formControlData.fieldName = name;
  }

  const value = (domNode as HTMLInputElement).value;
  if (value !== undefined) {
    let needRedaction = false;
    if (isPasswordField(domNode, tagName)) {
      needRedaction = !!value;
      // Exclude password field value mirroring Blink's logic.
      formControlData.redactionDecision = needRedaction ?
          PageContentRedactionDecision.REDACTED_HAS_BEEN_PASSWORD :
          PageContentRedactionDecision.UNREDACTED_EMPTY_PASSWORD;
    }
    if (!needRedaction) {
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
 * Extracts table name from a given table DOM Node.
 *
 * @param domNode The table element to process.
 * @return The populated PageContentTableData.
 */
function getTableNameForTableNode(domNode: HTMLElement): PageContentTableData {
  const tableData: PageContentTableData = {};
  const tableElement = domNode as HTMLTableElement;
  // NOTE: Table names will appear twice in the APC tree(once as a part of a
  // table node and once as a part of a text node). This matches Blink's
  // behavior.
  const tableName = tableElement.caption?.innerText?.trim();
  if (tableName) {
    tableData.tableName = tableName;
  }
  return tableData;
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
    domNode: HTMLElement, nonce: string, depth: number, maxDepth: number,
    actionableMode: boolean,
    paidContentContext: PaidContentExtractionContext): PageContentNode|null {
  const tagName = getStandardTagName(domNode);

  switch (tagName) {
    // 1. Complex Elements.
    case TAG_IFRAME:
      return getContentForIframeNode(
          domNode as HTMLIFrameElement, nonce, depth, maxDepth, actionableMode,
          paidContentContext);
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
    case TAG_SVG: {
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.SVG_ROOT,
        },
      };
    }

    // 2. Structural & Layout Elements.
    case TAG_TABLE: {
      return {
        childrenNodes: [],
        contentAttributes: {
          ...BASIC_CONTENT_ATTRIBUTES,
          attributeType: PageContentAttributeType.TABLE,
          tableData: getTableNameForTableNode(domNode),
        },
      };
    }
    case TAG_TR: {
      let rowType = PageContentTableRowType.BODY;
      // Use closest to find the nearest table section or table ancestor.
      // This handles cases where TR might be nested in a generic container
      // within a section. We include 'table' to ensure we stop at the nearest
      // table boundary and don't match a section from an outer table if this
      // row is inside a nested table.
      const section = domNode.closest('thead, tfoot, table');
      if (section && getStandardTagName(section) === 'THEAD') {
        rowType = PageContentTableRowType.HEADER;
      } else if (section && getStandardTagName(section) === 'TFOOT') {
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
 * @param maxDepth Maximal depth for nesting json objects beyond which content
 *     is truncated.
 * @param annotatedRoles The pre-calculated annotated roles which will be merged
 *     into the content attributes of the generated node.
 * @param interactionInfo The pre-calculated interaction info which will be
 *     merged into the content attributes of the generated node.
 * @return The populated PageContentNode or null if element should be skipped.
 */
function getContentForElementNode(
    domNode: HTMLElement, nonce: string, depth: number, maxDepth: number,
    annotatedRoles: PageContentAnnotatedRole[],
    interactionInfo: PageContentNodeInteractionInfo|undefined,
    actionableMode: boolean, interactiveNodeIds: InteractiveNodeIds,
    paidContentContext: PaidContentExtractionContext): PageContentNode|null {

  let labelForDOMNodeID: number | undefined = undefined;
  if (actionableMode && getStandardTagName(domNode) === TAG_LABEL) {
    labelForDOMNodeID = getAssociatedControlDOMNodeID(
        domNode as HTMLLabelElement, interactiveNodeIds);
  }

  let contentNode: PageContentNode|null = null;

  // 1. Try to get basic content for non-generic elements.
  contentNode = getBasicContentForNonGenericElement(
      domNode, nonce, depth, maxDepth, actionableMode, paidContentContext);

  // 2. Fallback: Generic Container.
  if (!contentNode &&
      isGenericContainer(domNode, interactiveNodeIds, interactionInfo,
                         annotatedRoles, labelForDOMNodeID)) {
    contentNode = {
      childrenNodes: [],
      contentAttributes: {
        attributeType: PageContentAttributeType.CONTAINER,
        isAdRelated: false,
      },
    };
  }

  // TODO(crbug.com/495959941): Support ARIA custom form control semantics.

  // TODO(crbug.com/468852704): Populate the rest of the attributes on top of
  // `basicAttributes`.

  if (contentNode) {
    if (annotatedRoles.length > 0) {
      contentNode.contentAttributes.annotatedRoles = annotatedRoles;
    }
    if (interactionInfo) {
      contentNode.contentAttributes.nodeInteractionInfo = interactionInfo;
    }

    if (labelForDOMNodeID !== undefined) {
      contentNode.contentAttributes.labelForDomNodeId = labelForDOMNodeID;
    }

    const ariaLabel = getAriaLabel(domNode);
    if (ariaLabel) {
      contentNode.contentAttributes.label = ariaLabel;
    }
  }

  return contentNode;
}

/**
 * Appends the annotated roles for the element, including tag-based roles,
 * ARIA roles, content-visibility states, and paid content roles to
 * the provided array.
 *
 * @param domNode The element to check.
 * @param annotatedRoles The array to populate with roles.
 * @param paidContentContext Context regarding paid content.
 */
function addAnnotatedRoles(
    domNode: HTMLElement,
    annotatedRoles: PageContentAnnotatedRole[],
    paidContentContext: PaidContentExtractionContext): void {
  const windowObj = domNode.ownerDocument?.defaultView;
  if (windowObj) {
    const style = windowObj.getComputedStyle(domNode);
    if (style.contentVisibility === STYLE_VALUE_CONTENT_VISIBILITY_HIDDEN) {
      annotatedRoles.push(PageContentAnnotatedRole.CONTENT_HIDDEN);
    }
  }

  const roleFromTag = getAnnotatedRoleForTag(getStandardTagName(domNode));
  if (roleFromTag !== null) {
    annotatedRoles.push(roleFromTag);
  }

  const ariaRoleAttr = domNode.getAttribute(ATTR_KEY_ROLE);
  if (ariaRoleAttr) {
    const roleFromAria = getAnnotatedRoleForAriaRole(ariaRoleAttr);
    if (roleFromAria !== undefined && !annotatedRoles.includes(roleFromAria)) {
      annotatedRoles.push(roleFromAria);
    }
  }

  if (paidContentContext.paidNodes.has(domNode)) {
    annotatedRoles.push(PageContentAnnotatedRole.PAID_CONTENT);
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
 * @param maxDepth The maximum recursion depth.
 * @param interactiveNodeIds A map of interactive node IDs to their
 *     interaction info.
 * @param actionableMode Whether to extract actionable interaction info.
 * @param paidContentContext Context regarding paid content.
 * @param hasCanvas Whether there is a canvas element on the page.
 * @return A new PageContentNode if valid content was found, null otherwise.
 */
function maybeGenerateContentNode(
    domNode: Node, nonce: string, depth: number, maxDepth: number,
    interactiveNodeIds: InteractiveNodeIds, actionableMode: boolean,
    paidContentContext: PaidContentExtractionContext,
    hasCanvas: boolean): PageContentNode|null {
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
    const annotatedRoles: PageContentAnnotatedRole[] = [];
    addAnnotatedRoles(element, annotatedRoles, paidContentContext);
    const interactionInfo =
        getNodeInteractionInfo(element, actionableMode, hasCanvas);

    const contentNode = getContentForElementNode(
        element, nonce, depth, maxDepth, annotatedRoles, interactionInfo,
        actionableMode, interactiveNodeIds, paidContentContext);
    if (contentNode) {
      const domNodeId = getOrCreateNodeId(domNode);
      if (domNodeId !== null) {
        contentNode.contentAttributes.domNodeId = domNodeId;
      }

      if (actionableMode) {
        const roleStr = element.getAttribute(ATTR_KEY_ROLE);
        contentNode.contentAttributes.ariaRole =
            roleStr ? getAXRoleForAriaRole(roleStr) : AxRole.AX_ROLE_UNKNOWN;
      }

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
    const tagName = getStandardTagName(element);
    if (TAGS_TO_REJECT.includes(tagName)) {
      return NodeFilter.FILTER_REJECT;
    }
    const windowObj = element.ownerDocument?.defaultView;
    if (!windowObj) {
      return NodeFilter.FILTER_REJECT;
    }
    const style = windowObj.getComputedStyle(element);
    if (style.display === ATTR_DISPLAY_NONE) {
      // Ignore the nodes and all their descendants that do not have
      // any display style which means that they would not have a
      // corresponding LayoutObject in blink.
      return NodeFilter.FILTER_REJECT;
    }
    if (style.visibility === ATTR_VISIBILITY_HIDDEN) {
      // Strictly skip invisible leaf nodes.
      if (TAGS_TO_STRICTLY_REJECT_IF_HIDDEN.includes(tagName)) {
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
    if (style.display === ATTR_DISPLAY_NONE ||
        style.visibility === ATTR_VISIBILITY_HIDDEN) {
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
 * @param interactiveNodeIds Specific node IDs verified as interactive.
 * @param actionableMode Whether to extract actionable interaction info.
 * @param paidContentContext Context regarding paid content.
 * @param hasCanvas Whether there is a canvas element on the page.
 */
function generateAndPushContentNode(
    node: Node, nonce: string, maxDepth: number,
    ancestorStack: AncestorStackItem[], interactiveNodeIds: InteractiveNodeIds,
    actionableMode: boolean, paidContentContext: PaidContentExtractionContext,
    hasCanvas: boolean) {
  const parentStackItem = ancestorStack[ancestorStack.length - 1]!;

  // 2. Generate Content Node. Skip nodes that are too deep while keep
  // walking the tree since future nodes might be shallow enough.
  const currentDepth = parentStackItem.depth + APC_NODE_DEPTH_COST;
  if (currentDepth > maxDepth) {
    // Ignore the node if it exceeds the max depth.
    return;
  }

  const newApcNode = maybeGenerateContentNode(
      node, nonce, currentDepth, maxDepth, interactiveNodeIds, actionableMode,
      paidContentContext, hasCanvas);
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

/**
 * Returns the standardized, uppercase tag name for an element.
 *
 * @param element The DOM element to evaluate.
 * @return The uppercase tag name.
 */
function getStandardTagName(element: Element): string {
  return element.tagName.toUpperCase();
}

/**
 * Returns the DOM node ID of the control element associated with a
 * given <label>.
 * Note: The returned node ID is the ID of the node that the
 * label is 'for', not the ID of the label element itself.
 *
 * @param labelElement The <label> element to inspect.
 * @param interactiveNodeIds The set of interactive node IDs to update.
 * @return The DOM node ID of the associated control, or undefined if
 *         none exists.
 */
function getAssociatedControlDOMNodeID(
    labelElement: HTMLLabelElement,
    interactiveNodeIds: InteractiveNodeIds,
): number | undefined {
  const associatedControl = labelElement.control;
  if (associatedControl) {
    const controlId = getOrCreateNodeId(associatedControl);
    if (controlId !== null) {
      interactiveNodeIds.add(controlId);
      return controlId;
    }
  }
  return undefined;
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
    document: Document, nonce: string, depth: number = 0, maxDepth: number,
    actionableMode: boolean, extractPaidContent: boolean,
    attemptPaidContentJsonFixing: boolean): PageContent|null {
  if (depth > maxDepth) {
    return null;
  }

  const documentWindow = document.defaultView;
  if (!documentWindow) {
    // A document without a window doesn't have any value.
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

  // TODO(crbug.com/480945289): Assume there is a canvas when feature is
  // disabled. We only need to extract the scroller info for nodes when there is
  // a canvas on the page. It is required to compute the canvas heavy heuristic.
  const hasCanvas = isPageContextIPCOptimizationEnabled() ?
      document.querySelector('canvas') !== null :
      true;

  // Perform pre-walk extraction of paid content globals and specific nodes.
  const paidContentContext = extractContainsPaidContent(
      document, extractPaidContent, attemptPaidContentJsonFixing);

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
        currentNode, nonce, maxDepth, ancestorStack, interactiveNodeIds,
        actionableMode, paidContentContext, hasCanvas);

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

  // Start the viewport at (0, 0) as it represents the entire page surface which
  // is the root surface. This deliberately extracts the layout viewport bounds,
  // rather than accounting for visual viewport offsets (e.g., pinch-to-zoom),
  // to maintain parity with Blink's ConvertViewportGeometry in
  // components/optimization_guide/content/browser/page_content_proto_provider.cc.
  const viewportGeometry = {
    x: 0,
    y: 0,
    width: documentWindow.innerWidth,
    height: documentWindow.innerHeight,
  };

  return {
    rootNode,
    pageInteractionInfo,
    frameData: extractFrameData(document, paidContentContext),
    viewportGeometry,
    visibleBoundingBoxesForPasswordRedaction: [],
  };
}
