// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview TypeScript definitions for ai_page_content.mojom.
 * This file matches the structure of the Mojo definitions.
 * The structures defined here are intended to be used to convey the annotated
 * page content between the webkit renderer and the browser through IPC calls.
 * Read the mojom files to get more documentation on these.
 */

// Definitions from mojo/public/mojom/base/unguessable_token.mojom

// TODO(crbug.com/473752555): Serialize the UnguessableToken as a 2 numbers
// struct instead of a string to use less bandwidth.
//
// Use the string format instead of the UnguessableToken 2 numbers format since
// this is the standard format we use to serialize these type of tokens in ios.
export type UnguessableToken = string;

// Definitions from
// ui/gfx/geometry/mojom/geometry.mojom

export interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

export interface Point {
  x: number;
  y: number;
}

export interface Size {
  width: number;
  height: number;
}

// Definitions from url/mojom/origin.mojom

export interface Origin {
  scheme: string;
  host: string;
  port: number;
  nonceIfOpaque?: UnguessableToken;
}

// Definitions from third_party/blink/public/mojom/tokens/tokens.mojom

export interface FrameToken {
  value: UnguessableToken;
}

// Definitions from
//  third_party/blink/public/mojom/forms/form_control_type.mojom

// The numbers are aligned with the FormControlType enum in
// components/optimization_guide/proto/features/common_quality_data.proto.
export enum FormControlType {
  BUTTON_BUTTON = 16,
  BUTTON_SUBMIT = 17,
  BUTTON_RESET = 18,
  BUTTON_POPOVER = 19,
  FIELDSET = 20,
  INPUT_BUTTON = 21,
  INPUT_CHECKBOX = 2,
  INPUT_COLOR = 22,
  INPUT_DATE = 23,
  INPUT_DATETIME_LOCAL = 24,
  INPUT_EMAIL = 3,
  INPUT_FILE = 25,
  INPUT_HIDDEN = 26,
  INPUT_IMAGE = 27,
  INPUT_MONTH = 4,
  INPUT_NUMBER = 5,
  INPUT_PASSWORD = 6,
  INPUT_RADIO = 7,
  INPUT_RANGE = 28,
  INPUT_RESET = 29,
  INPUT_SEARCH = 8,
  INPUT_SUBMIT = 30,
  INPUT_TELEPHONE = 9,
  INPUT_TEXT = 10,
  INPUT_TIME = 31,
  INPUT_URL = 11,
  INPUT_WEEK = 32,
  OUTPUT = 33,
  SELECT_ONE = 12,
  SELECT_MULTIPLE = 13,
  TEXT_AREA = 15,
}

// Definitions from
// third_party/blink/public/mojom/content_extraction/ai_page_content.mojom

// The numbers are aligned with the ContentAttributeType enum in
// components/optimization_guide/proto/features/common_quality_data.proto.
export enum PageContentAttributeType {
  UNKNOWN = 0,  // DO NOT USE
  ROOT = 1,
  CONTAINER = 2,
  IFRAME = 3,
  PARAGRAPH = 4,
  HEADING = 5,
  ORDERED_LIST = 6,
  UNORDERED_LIST = 7,
  FORM = 8,
  IMAGE = 9,
  TABLE = 10,
  TABLE_CELL = 11,
  TEXT = 20,
  TABLE_ROW = 21,
  ANCHOR = 22,
  LIST_ITEM = 23,
  FORM_CONTROL = 24,
  SVG_ROOT = 25,
  CANVAS = 26,
  VIDEO = 27,
}

// The numbers are aligned with the AnnotatedRole enum in
// components/optimization_guide/proto/features/common_quality_data.proto.
export enum PageContentAnnotatedRole {
  HEADER = 5,
  NAV = 6,
  SEARCH = 7,
  MAIN = 8,
  ARTICLE = 9,
  SECTION = 10,
  ASIDE = 11,
  FOOTER = 12,
  CONTENT_HIDDEN = 13,
  PAID_CONTENT = 14,
}

export interface PageContentGeometry {
  outerBoundingBox: Rect;
  visibleBoundingBox?: Rect;
  fragmentVisibleBoundingBoxes?: Rect[];
}

export interface PageContentSelection {
  startDomNodeId: number;
  startOffset: number;
  endDomNodeId: number;
  endOffset: number;
  selectedText: string;
}

export interface PageContentPageInteractionInfo {
  focusedDomNodeId?: number;
  accessibilityFocusedDomNodeId?: number;
  mousePosition?: Point;
}

export interface PageContentFrameInteractionInfo {
  focusedDomNodeId?: number;
  selection?: PageContentSelection;
}

// The numbers are aligned with the ClickabilityReason enum in
// components/optimization_guide/proto/features/common_quality_data.proto.
export enum PageContentClickabilityReason {
  CLICKABLE_CONTROL = 0,
  CLICK_EVENTS = 1,
  MOUSE_EVENTS = 2,  // Deprecated in proto
  MOUSE_CLICK = 12,
  MOUSE_HOVER = 13,
  KEY_EVENTS = 3,
  EDITABLE = 4,
  CURSOR_POINTER = 5,
  ARIA_ROLE = 6,
  ARIA_HAS_POPUP = 7,
  ARIA_EXPANDED_TRUE = 8,
  ARIA_EXPANDED_FALSE = 9,
  TAB_INDEX = 10,
  AUTOCOMPLETE = 11,
  HOVER_PSEUDO_CLASS = 14,
}

// The numbers are aligned with the InteractionDisabledReason enum in
// components/optimization_guide/proto/features/common_quality_data.proto.
export enum PageContentInteractionDisabledReason {
  DISABLED = 1,
  ARIA_DISABLED = 2,
  CURSOR_NOT_ALLOWED = 3,
}

export interface PageContentNodeInteractionInfo {
  scrollerInfo?: PageContentScrollerInfo;
  isFocusable: boolean;
  documentScopedZOrder?: number;
  clickabilityReasons: PageContentClickabilityReason[];
  isDisabled: boolean;
  interactionDisabledReasons: PageContentInteractionDisabledReason[];
}

export interface PageContentScrollerInfo {
  scrollingBounds: Size;
  visibleArea: Rect;
  userScrollableHorizontal: boolean;
  userScrollableVertical: boolean;
}

// The numbers are aligned with the TextSize enum in
// components/optimization_guide/proto/features/common_quality_data.proto.
export enum PageContentTextSize {
  XS = 1,
  S = 2,
  M = 0,  // DEFAULT
  L = 3,
  XL = 4,
}

export interface PageContentTextStyle {
  textSize: PageContentTextSize;
  hasEmphasis: boolean;
  // Color is passed as a string to avoid 32-bit signed integer overflow
  // during bridge conversion (values can exceed INT_MAX).
  color?: string;
}

export interface PageContentTextInfo {
  textContent: string;
  textStyle: PageContentTextStyle;
}

// The numbers are aligned with the AnchorRel enum in
// components/optimization_guide/proto/features/common_quality_data.proto.
export enum PageContentAnchorRel {
  RELATION_UNKNOWN = 0,
  RELATION_NO_REFERRER = 1,
  RELATION_NO_OPENER = 2,
  RELATION_OPENER = 3,
  RELATION_PRIVACY_POLICY = 4,
  RELATION_TERMS_OF_SERVICE = 5,
}

export interface PageContentAnchorData {
  url: string;
  rel: PageContentAnchorRel[];
}

export interface PageContentImageInfo {
  imageCaption?: string;
  sourceOrigin?: Origin;
}

export interface PageContentSvgRootData {
  innerText?: string;
}

export interface PageContentCanvasData {
  layoutSize: Size;
}

export interface PageContentVideoData {
  url: string;
  sourceOrigin?: Origin;
}

export interface PageContentMeta {
  name: string;
  content: string;
}

// The numbers are aligned with the MediaDataType enum in
// components/optimization_guide/proto/features/common_quality_data.proto.
export enum PageContentMediaType {
  MEDIA_DATA_TYPE_UNKNOWN = 0,
  MEDIA_DATA_TYPE_VIDEO = 1,
  MEDIA_DATA_TYPE_AUDIO = 2,
}

export interface PageContentMediaData {
  mediaDataType: PageContentMediaType;
  durationMilliseconds: number;
  currentPositionMilliseconds: number;
  isPlaying: boolean;
}

// Some fields aren't listed here because they are not supported on ios:
//   - scriptTools
export interface PageContentFrameData {
  frameInteractionInfo: PageContentFrameInteractionInfo;
  metaData: PageContentMeta[];
  title?: string;
  containsPaidContent?: boolean;
  popup?: PageContentPopup;
  // Exclusive to ios which needs to get the full url from JS to get more than
  // the URL origin from the WebFrame data.
  sourceUrl?: string;
  // Exclusive to ios which gets the document id from the remote token issued
  // during iframe registration. Just populated for PageContentIframeContent.
  documentId?: string;
  mediaData?: PageContentMediaData;
  isFocusedDocument?: boolean;
}

// The numbers are aligned with the RedactedFrameMetadata enum in
// components/optimization_guide/proto/features/common_quality_data.proto.
export namespace RedactedFrameMetadata {
  export enum Reason {
    CROSS_SITE = 1,
    CROSS_ORIGIN = 2,
  }
}

export interface RedactedFrameMetadata {
  reason: RedactedFrameMetadata.Reason;
}

export interface PageContentIframeContent {
  localFrameData?: PageContentFrameData;
  redactedFrameMetadata?: RedactedFrameMetadata;
}

export interface PageContentIframeData {
  // The token used by the browser to identify and graft this child frame.
  remoteFrameToken: FrameToken;
  // The frame's local and private token in the renderer context.
  localFrameToken?: FrameToken;
  // The grafted iframe content if same-origin.
  content?: PageContentIframeContent;
}

export interface PageContentTableData {
  tableName?: string;
}

// The numbers are aligned with the TableRowType enum in
// components/optimization_guide/proto/features/common_quality_data.proto.
export enum PageContentTableRowType {
  HEADER = 1,
  BODY = 2,
  FOOTER = 3,
}

export interface PageContentTableRowData {
  rowType: PageContentTableRowType;
}

export interface PageContentFormData {
  formName?: string;
  actionUrl?: string;
}

export interface PageContentSelectOption {
  value?: string;
  text?: string;
  isSelected: boolean;
  disabled: boolean;
}

// The numbers are aligned with the RedactionDecision enum in
// components/optimization_guide/proto/features/common_quality_data.proto.
export enum PageContentRedactionDecision {
  NO_REDACTION_NECESSARY = 0,
  UNREDACTED_EMPTY_PASSWORD = 1,
  REDACTED_HAS_BEEN_PASSWORD = 2,
}

export interface PageContentFormControlData {
  formControlType: FormControlType;
  fieldName?: string;
  fieldValue?: string;
  selectOptions: PageContentSelectOption[];
  placeholder?: string;
  isChecked: boolean;
  isRequired: boolean;
  isReadonly?: boolean;
  redactionDecision: PageContentRedactionDecision;
  // Node ID assigned by Autofill if the node is handled by Autofill.
  autofillNodeId?: number;
}

export interface PageContentAttributes {
  domNodeId?: number;
  ariaRole?: AxRole;
  attributeType: PageContentAttributeType;
  geometry?: PageContentGeometry;
  nodeInteractionInfo?: PageContentNodeInteractionInfo;
  textInfo?: PageContentTextInfo;
  imageInfo?: PageContentImageInfo;
  svgRootData?: PageContentSvgRootData;
  canvasData?: PageContentCanvasData;
  videoData?: PageContentVideoData;
  anchorData?: PageContentAnchorData;
  formData?: PageContentFormData;
  formControlData?: PageContentFormControlData;
  tableData?: PageContentTableData;
  iframeData?: PageContentIframeData;
  tableRowData?: PageContentTableRowData;
  annotatedRoles?: PageContentAnnotatedRole[];
  label?: string;
  labelForDomNodeId?: number;
  isAdRelated: boolean;
}

export interface PageContentNode {
  childrenNodes: PageContentNode[];
  contentAttributes: PageContentAttributes;
}

export interface PageContentPopup {
  rootNode: PageContentNode;
  openerDomNodeId: number;
  visibleBoundingBox: Rect;
}

export interface PageContent {
  rootNode: PageContentNode;
  pageInteractionInfo?: PageContentPageInteractionInfo;
  frameData: PageContentFrameData;
  viewportGeometry: Rect;
  visibleBoundingBoxesForPasswordRedaction: Rect[];
}

// The numbers are aligned with the AnnotatedPageContentMode enum in
// components/optimization_guide/proto/features/common_quality_data.proto.
export enum PageContentMode {
  DEFAULT = 0,
  ACTIONABLE_ELEMENTS = 1,
}

export interface PageContentOptions {
  mode: PageContentMode;
  onCriticalPath: boolean;
  maxMetaElements: number;
  includeSameSiteOnly: boolean;
  mainFrameViewRectInDips: Rect;
  includePasswordsForRedaction: boolean;
}

/**
 * Enum representing accessibility roles.
 * The numbers are aligned with the AxRole enum in
 * ui/accessibility/ax_enums.mojom.
 */

export enum AxRole {
  AX_ROLE_NONE = 0,
  AX_ROLE_UNKNOWN = 181, // The role has not been set.
  AX_ROLE_BANNER = 7,
  AX_ROLE_BUTTON = 9,
  AX_ROLE_CHECK_BOX = 14,
  AX_ROLE_LINK = 110,
  AX_ROLE_MENU_ITEM = 124,
  AX_ROLE_MENU_ITEM_CHECK_BOX = 125,
  AX_ROLE_MENU_ITEM_RADIO = 126,
  AX_ROLE_LIST_BOX_OPTION = 113,
  AX_ROLE_RADIO_BUTTON = 141,
  AX_ROLE_SWITCH = 163,
  AX_ROLE_TAB = 164,
  AX_ROLE_NAVIGATION = 130,
  AX_ROLE_SEARCH = 152,
  AX_ROLE_MAIN = 118,
  AX_ROLE_ARTICLE = 5,
  AX_ROLE_REGION = 143,
  AX_ROLE_COMPLEMENTARY = 22,
  AX_ROLE_CONTENT_INFO = 26,
}
