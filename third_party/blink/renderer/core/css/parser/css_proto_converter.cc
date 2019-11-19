// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include "third_party/blink/renderer/core/css/parser/css_proto_converter.h"

// TODO(metzman): Figure out how to remove this include and use DCHECK.
#include "third_party/blink/renderer/core/css/parser/css.pb.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace css_proto_converter {

const int Converter::kAtRuleDepthLimit = 5;
const int Converter::kSupportsConditionDepthLimit = 5;

const std::string Converter::kViewportPropertyLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "min-width",  "max-width", "width",       "min-height",
    "max-height", "height",    "zoom",        "min-zoom",
    "user-zoom",  "max-zoom",  "orientation",
};

const std::string Converter::kViewportValueLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "landscape", "portrait", "auto", "zoom", "fixed", "none",
};

const std::string Converter::kPseudoLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "-internal-autofill-previewed",
    "-internal-autofill-selected",
    "-internal-is-html",
    "-internal-list-box",
    "-internal-media-controls-overlay-cast-button",
    "-internal-multi-select-focus",
    "-internal-shadow-host-has-appearance",
    "-internal-spatial-navigation-focus",
    "-internal-video-persistent",
    "-internal-video-persistent-ancestor",
    "-internal-xr-immersive-dom-overlay",
    "-webkit-any-link",
    "-webkit-autofill",
    "-webkit-drag",
    "-webkit-full-page-media",
    "-webkit-full-screen",
    "-webkit-full-screen-ancestor",
    "-webkit-resizer",
    "-webkit-scrollbar",
    "-webkit-scrollbar-button",
    "-webkit-scrollbar-corner",
    "-webkit-scrollbar-thumb",
    "-webkit-scrollbar-track",
    "-webkit-scrollbar-track-piece",
    "active",
    "after",
    "backdrop",
    "before",
    "checked",
    "content",
    "corner-present",
    "cue",
    "decrement",
    "default",
    "defined",
    "disabled",
    "double-button",
    "empty",
    "enabled",
    "end",
    "first",
    "first-child",
    "first-letter",
    "first-line",
    "first-of-type",
    "focus",
    "focus-within",
    "fullscreen",
    "future",
    "horizontal",
    "host",
    "hover",
    "in-range",
    "increment",
    "indeterminate",
    "invalid",
    "last-child",
    "last-of-type",
    "left",
    "link",
    "no-button",
    "only-child",
    "only-of-type",
    "optional",
    "out-of-range",
    "past",
    "placeholder",
    "placeholder-shown",
    "read-only",
    "read-write",
    "required",
    "right",
    "root",
    "scope",
    "selection",
    "shadow",
    "single-button",
    "start",
    "target",
    "unresolved",
    "valid",
    "vertical",
    "visited",
    "window-inactive",
    "-webkit-any",
    "host-context",
    "lang",
    "not",
    "nth-child",
    "nth-last-child",
    "nth-last-of-type",
    "nth-of-type",
    "slotted",
    "INVALID_PSEUDO_VALUE"};

const std::string Converter::kMediaTypeLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "all",
    "braille",
    "embossed",
    "handheld",
    "print",
    "projection",
    "screen",
    "speech",
    "tty",
    "tv",
    "INVALID_MEDIA_TYPE"};

const std::string Converter::kMfNameLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "any-hover",
    "any-pointer",
    "color",
    "color-index",
    "color-gamut",
    "grid",
    "monochrome",
    "height",
    "hover",
    "width",
    "orientation",
    "aspect-ratio",
    "device-aspect-ratio",
    "-webkit-device-pixel-ratio",
    "device-height",
    "device-width",
    "display-mode",
    "max-color",
    "max-color-index",
    "max-aspect-ratio",
    "max-device-aspect-ratio",
    "-webkit-max-device-pixel-ratio",
    "max-device-height",
    "max-device-width",
    "max-height",
    "max-monochrome",
    "max-width",
    "max-resolution",
    "min-color",
    "min-color-index",
    "min-aspect-ratio",
    "min-device-aspect-ratio",
    "-webkit-min-device-pixel-ratio",
    "min-device-height",
    "min-device-width",
    "min-height",
    "min-monochrome",
    "min-width",
    "min-resolution",
    "pointer",
    "resolution",
    "-webkit-transform-3d",
    "scan",
    "shape",
    "immersive",
    "INVALID_NAME"};

const std::string Converter::kImportLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "'custom.css'", "url(\"chrome://communicator/skin/\")"};

const std::string Converter::kEncodingLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "UTF-8", "UTF-16", "UTF-32",
};

const std::string Converter::kValueLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "all",
    "dynamic",
    "yellow",
    "graytext",
    "color-dodge",
    "darkseagreen",
    "disc",
    "extra-condensed",
    "hanging",
    "step-middle",
    "menulist",
    "row",
    "pre-wrap",
    "inline-block",
    "step-start",
    "isolate-override",
    "swap",
    "rtl",
    "crimson",
    "tb",
    "common-ligatures",
    "-webkit-min-content",
    "brown",
    "khmer",
    "infinite",
    "table-header-group",
    "before-edge",
    "read-write",
    "rl",
    "wavy",
    "proportional-width",
    "no-drop",
    "cyan",
    "difference",
    "exact",
    "square-button",
    "skyblue",
    "-webkit-isolate-override",
    "table-row-group",
    "darkgray",
    "button",
    "ethiopic-halehame-am",
    "large",
    "lightpink",
    "crosshair",
    "teal",
    "fill-box",
    "small",
    "media-sliderthumb",
    "round",
    "-internal-media-subtitles-icon",
    "media-play-button",
    "smaller",
    "jis04",
    "lr-tb",
    "lightgoldenrodyellow",
    "lavender",
    "ultra-expanded",
    "dimgrey",
    "diagonal-fractions",
    "blue",
    "xor",
    "sub",
    "telugu",
    "crispEdges",
    "-webkit-mini-control",
    "zoom-out",
    "searchfield",
    "cell",
    "gujarati",
    "above",
    "no-punctuation",
    "new",
    "filled",
    "use-script",
    "condensed",
    "loose",
    "source-out",
    "objects",
    "slider-horizontal",
    "row-resize",
    "break-all",
    "wait",
    "media-exit-fullscreen-button",
    "korean-hangul-formal",
    "floralwhite",
    "reset-size",
    "zoom-in",
    "-webkit-grabbing",
    "larger",
    "max-content",
    "sRGB",
    "literal-punctuation",
    "windowframe",
    "subpixel-antialiased",
    "buttonhighlight",
    "hue",
    "pixelated",
    "sticky",
    "greenyellow",
    "linearRGB",
    "lightseagreen",
    "logical",
    "-webkit-right",
    "sienna",
    "flow-root",
    "optimizeSpeed",
    "korean-hanja-formal",
    "nowrap",
    "x-small",
    "landscape",
    "lime",
    "x-large",
    "ns-resize",
    "appworkspace",
    "peru",
    "all-petite-caps",
    "black",
    "xx-small",
    "all-scroll",
    "darkslategray",
    "flat",
    "georgian",
    "under",
    "lemonchiffon",
    "chocolate",
    "pre-line",
    "context-menu",
    "darkgrey",
    "view-box",
    "olive",
    "-webkit-plaintext",
    "extra-expanded",
    "antiquewhite",
    "none",
    "moccasin",
    "local",
    "stroke",
    "darkslateblue",
    "lightskyblue",
    "content-box",
    "thin",
    "deeppink",
    "spell-out",
    "non-scaling-stroke",
    "slider-vertical",
    "-webkit-box",
    "plum",
    "-internal-media-overlay-cast-off-button",
    "inactivecaptiontext",
    "dodgerblue",
    "threedshadow",
    "petite-caps",
    "paused",
    "-webkit-link",
    "message-box",
    "-internal-center",
    "triangle",
    "magenta",
    "tan",
    "absolute",
    "pink",
    "hiragana-iroha",
    "farthest-side",
    "palevioletred",
    "close-quote",
    "threedlightshadow",
    "caption",
    "powderblue",
    "table-column",
    "source-atop",
    "hiragana",
    "upper-armenian",
    "windowtext",
    "full-width",
    "progress-bar-value",
    "midnightblue",
    "inline-flex",
    "economy",
    "lao",
    "clone",
    "after",
    "status-bar",
    "lowercase",
    "mixed",
    "line-through",
    "lightslategray",
    "small-caption",
    "infobackground",
    "discard",
    "captiontext",
    "end",
    "-internal-inactive-list-box-selection-text",
    "capitalize",
    "mediumseagreen",
    "tomato",
    "cadetblue",
    "decimal-leading-zero",
    "sans-serif",
    "linen",
    "green",
    "inactiveborder",
    "inline",
    "fallback",
    "peachpuff",
    "-webkit-max-content",
    "plus-lighter",
    "checkbox",
    "help",
    "oblique",
    "move",
    "meter",
    "ledger",
    "slategrey",
    "media-time-remaining-display",
    "urdu",
    "pointer",
    "before",
    "darkslategrey",
    "-webkit-control",
    "-webkit-inline-box",
    "hard-light",
    "miter",
    "oriya",
    "upper-latin",
    "window",
    "mediumblue",
    "lr",
    "orange",
    "hidden",
    "-internal-active-list-box-selection",
    "bolder",
    "-webkit-center",
    "safe",
    "highlighttext",
    "accumulate",
    "flex-end",
    "transparent",
    "-internal-media-remoting-cast-icon",
    "goldenrod",
    "historical-ligatures",
    "darkviolet",
    "always",
    "decimal",
    "block-axis",
    "scrollbar",
    "ew-resize",
    "darkmagenta",
    "not-allowed",
    "ease-in",
    "table-column-group",
    "square",
    "no-contextual",
    "-webkit-fill-available",
    "frames",
    "persian",
    "static",
    "navy",
    "visiblePainted",
    "thick",
    "simp-chinese-formal",
    "ghostwhite",
    "space",
    "darkkhaki",
    "keep-all",
    "content",
    "-internal-media-download-button",
    "upper-roman",
    "cornsilk",
    "red",
    "no-change",
    "linear",
    "-internal-media-control",
    "sideways",
    "contain",
    "katakana-iroha",
    "steelblue",
    "double-circle",
    "antialiased",
    "aliceblue",
    "lightslategrey",
    "geometricPrecision",
    "gainsboro",
    "inline-table",
    "ltr",
    "backwards",
    "s-resize",
    "lightgrey",
    "media-mute-button",
    "listitem",
    "mistyrose",
    "darksalmon",
    "sideways-right",
    "jis83",
    "mediumspringgreen",
    "caps-lock-indicator",
    "sliderthumb-horizontal",
    "forwards",
    "upper-alpha",
    "blink",
    "fantasy",
    "simplified",
    "orangered",
    "navajowhite",
    "open",
    "horizontal",
    "slategray",
    "activecaption",
    "korean-hanja-informal",
    "strict",
    "lightcyan",
    "top",
    "-webkit-pictograph",
    "white",
    "text-after-edge",
    "lightgray",
    "collapse",
    "hover",
    "-webkit-optimize-contrast",
    "padding",
    "butt",
    "off",
    "thai",
    "copy",
    "hotpink",
    "double",
    "lower-greek",
    "grey",
    "media-volume-slider-container",
    "-webkit-inline-flex",
    "space-evenly",
    "activeborder",
    "browser",
    "pre",
    "unicase",
    "simp-chinese-informal",
    "clip",
    "closest-corner",
    "plaintext",
    "no-repeat",
    "text-top",
    "jis78",
    "xx-large",
    "rl-tb",
    "table-row",
    "medium",
    "mongolian",
    "katakana",
    "element",
    "border",
    "rosybrown",
    "progress-bar",
    "whitesmoke",
    "lightblue",
    "-webkit-left",
    "no-common-ligatures",
    "listbox",
    "isolate",
    "snow",
    "step-end",
    "ethiopic-halehame-ti-er",
    "ethiopic-halehame-ti-et",
    "multiple",
    "-internal-inactive-list-box-selection",
    "normal",
    "blueviolet",
    "salmon",
    "lower-alpha",
    "oldlace",
    "letter",
    "border-box",
    "alpha",
    "tibetan",
    "icon",
    "flex-start",
    "textarea",
    "w-resize",
    "clear",
    "cover",
    "farthest-corner",
    "menulist-textfield",
    "traditional",
    "left",
    "dot",
    "luminance",
    "gold",
    "show",
    "text",
    "-webkit-match-parent",
    "radio",
    "cambodian",
    "repeat-x",
    "repeat-y",
    "fine",
    "textfield",
    "from-image",
    "lining-nums",
    "menu",
    "proportional-nums",
    "source-over",
    "ne-resize",
    "papayawhip",
    "source-in",
    "se-resize",
    "circle",
    "destination-out",
    "threedface",
    "over",
    "distribute",
    "inactivecaption",
    "lighten",
    "-webkit-fit-content",
    "lighter",
    "contextual",
    "gray",
    "darkturquoise",
    "e-resize",
    "luminosity",
    "list-item",
    "limegreen",
    "fixed",
    "min-content",
    "media-slider",
    "visibleStroke",
    "cubic-bezier",
    "closest-side",
    "relative",
    "no-open-quote",
    "thistle",
    "violet",
    "portrait",
    "fullscreen",
    "honeydew",
    "on-demand",
    "cornflowerblue",
    "darkblue",
    "outside",
    "progress",
    "mediumpurple",
    "darkcyan",
    "vertical",
    "monospace",
    "break-word",
    "screen",
    "rebeccapurple",
    "darkred",
    "vertical-lr",
    "optimizeQuality",
    "armenian",
    "nwse-resize",
    "text-before-edge",
    "optional",
    "exclusion",
    "both",
    "mediumturquoise",
    "lower-roman",
    "reverse",
    "hangul-consonant",
    "soft-light",
    "aqua",
    "button-bevel",
    "gurmukhi",
    "lightsteelblue",
    "small-caps",
    "n-resize",
    "table-footer-group",
    "destination-in",
    "olivedrab",
    "read-write-plaintext-only",
    "padding-box",
    "col-resize",
    "-internal-media-track-selection-checkmark",
    "lower-latin",
    "-webkit-nowrap",
    "table",
    "buttonshadow",
    "palegreen",
    "jis90",
    "fit-content",
    "stretch",
    "seashell",
    "threedhighlight",
    "visibleFill",
    "space-around",
    "coarse",
    "aquamarine",
    "digits",
    "currentcolor",
    "painted",
    "tb-rl",
    "buttonface",
    "lawngreen",
    "burlywood",
    "-webkit-small-control",
    "slateblue",
    "mintcream",
    "ruby",
    "solid",
    "ultra-condensed",
    "expanded",
    "saddlebrown",
    "vertical-rl",
    "sesame",
    "-webkit-body",
    "destination-atop",
    "malayalam",
    "wrap-reverse",
    "balance",
    "vertical-right",
    "no-close-quote",
    "flex",
    "push-button",
    "darkgoldenrod",
    "saturation",
    "middle",
    "sandybrown",
    "hebrew",
    "menutext",
    "inline-axis",
    "baseline",
    "-webkit-grab",
    "darkorange",
    "-webkit-flex",
    "nw-resize",
    "contents",
    "auto",
    "margin-box",
    "document",
    "palegoldenrod",
    "ordinal",
    "hand",
    "running",
    "cjk-earthly-branch",
    "table-caption",
    "media-toggle-closed-captions-button",
    "after-edge",
    "sliderthumb-vertical",
    "center",
    "lightyellow",
    "lavenderblush",
    "-internal-media-closed-captions-icon",
    "inherit",
    "media-controls-background",
    "justify",
    "optimizeLegibility",
    "-webkit-baseline-middle",
    "indigo",
    "minimal-ui",
    "firebrick",
    "indianred",
    "darkolivegreen",
    "semi-expanded",
    "underline",
    "myanmar",
    "space-between",
    "ease",
    "alternate",
    "mediumorchid",
    "silver",
    "color",
    "chartreuse",
    "ease-in-out",
    "springgreen",
    "lightsalmon",
    "turquoise",
    "hide",
    "horizontal-tb",
    "vertical-text",
    "alias",
    "grid",
    "no-discretionary-ligatures",
    "background",
    "devanagari",
    "text-bottom",
    "darkgreen",
    "visible",
    "tabular-nums",
    "manual",
    "zoom",
    "cjk-heavenly-stem",
    "steps",
    "bounding-box",
    "alphabetic",
    "after-white-space",
    "row-reverse",
    "media-current-time-display",
    "mathematical",
    "ethiopic-halehame",
    "right",
    "uppercase",
    "-webkit-xxx-large",
    "b4",
    "b5",
    "yellowgreen",
    "media-controls-fullscreen-background",
    "lower-armenian",
    "orchid",
    "nonzero",
    "slice",
    "dense",
    "inter-word",
    "bottom",
    "purple",
    "avoid",
    "separate",
    "hangul",
    "legal",
    "alternate-reverse",
    "preserve-3d",
    "read-only",
    "ellipsis",
    "media-overlay-play-button",
    "bisque",
    "infotext",
    "khaki",
    "wheat",
    "bold",
    "no-historical-ligatures",
    "bidi-override",
    "deepskyblue",
    "ease-out",
    "cjk-ideographic",
    "oldstyle-nums",
    "media-enter-fullscreen-button",
    "super",
    "cursive",
    "on",
    "central",
    "-internal-media-overflow-button",
    "standalone",
    "column",
    "coral",
    "destination-over",
    "discretionary-ligatures",
    "beige",
    "table-cell",
    "azure",
    "trad-chinese-informal",
    "titling-caps",
    "-webkit-zoom-in",
    "block",
    "outset",
    "mediumvioletred",
    "royalblue",
    "menulist-text",
    "sw-resize",
    "multiply",
    "threeddarkshadow",
    "wrap",
    "lightcoral",
    "ellipse",
    "-internal-active-list-box-selection-text",
    "ridge",
    "-webkit-auto",
    "-internal-quirk-inherit",
    "initial",
    "fuchsia",
    "menulist-button",
    "blanchedalmond",
    "caret",
    "start",
    "-internal-media-cast-off-button",
    "italic",
    "ivory",
    "buttontext",
    "semi-condensed",
    "inline-grid",
    "-webkit-activelink",
    "serif",
    "forestgreen",
    "bengali",
    "upright",
    "reset",
    "bevel",
    "ideographic",
    "darken",
    "media-volume-sliderthumb",
    "default",
    "inside",
    "below",
    "highlight",
    "embed",
    "groove",
    "nesw-resize",
    "stacked-fractions",
    "unsafe",
    "maroon",
    "kannada",
    "single",
    "at",
    "ink",
    "arabic-indic",
    "media-volume-slider",
    "column-reverse",
    "-webkit-zoom-out",
    "fill",
    "evenodd",
    "dotted",
    "dimgray",
    "dashed",
    "seagreen",
    "trad-chinese-formal",
    "mediumslateblue",
    "paleturquoise",
    "inner-spin-button",
    "repeat",
    "darkorchid",
    "-webkit-isolate",
    "searchfield-cancel-button",
    "all-small-caps",
    "a3",
    "a5",
    "a4",
    "open-quote",
    "lightgreen",
    "slashed-zero",
    "color-burn",
    "auto-flow",
    "overlay",
    "visual",
    "scale-down",
    "overline",
    "inset",
    "mediumaquamarine",
    "scroll",
    "layout",
    "break-spaces",
    "pan-left",
    "proximity",
    "inline-start",
    "pan-x",
    "grab",
    "grabbing",
    "inline-end",
    "pan-right",
    "jump-end",
    "manipulation",
    "pinch-zoom",
    "xxx-large",
    "pan-down",
    "anywhere",
    "jump-none",
    "drag",
    "avoid-page",
    "mandatory",
    "paint",
    "jump-both",
    "size",
    "style",
    "pan-y",
    "recto",
    "markers",
    "verso",
    "page",
    "pan-up",
    "avoid-column",
    "smooth",
    "jump-start",
    "no-drag",
    "INVALID_VALUE",
};

const std::string Converter::kPropertyLookupTable[] = {
    "",  // This is just to fill the zeroth spot. It should not be used.
    "all",
    "-webkit-animation-iteration-count",
    "font-feature-settings",
    "-webkit-text-emphasis-position",
    "-webkit-text-emphasis-style",
    "grid-template-rows",
    "text-underline-position",
    "-webkit-flex-grow",
    "scroll-margin-right",
    "-webkit-column-rule",
    "-webkit-order",
    "grid-row-gap",
    "row-gap",
    "backdrop-filter",
    "font-variant-east-asian",
    "buffered-rendering",
    "-webkit-appearance",
    "outline-width",
    "alignment-baseline",
    "-webkit-flex-flow",
    "column-rule",
    "grid-column-gap",
    "-webkit-border-after",
    "-webkit-column-break-inside",
    "-webkit-shape-outside",
    "-webkit-print-color-adjust",
    "list-style-type",
    "page-break-before",
    "flood-color",
    "text-anchor",
    "-webkit-padding-start",
    "-webkit-user-select",
    "-webkit-column-rule-color",
    "padding-left",
    "-webkit-backface-visibility",
    "-webkit-margin-before",
    "break-inside",
    "column-count",
    "-webkit-logical-height",
    "perspective",
    "max-block-size",
    "-webkit-animation-play-state",
    "border-image-repeat",
    "-webkit-font-size-delta",
    "scroll-padding-bottom",
    "border-right-style",
    "border-left-style",
    "scroll-margin-block",
    "flex-flow",
    "outline-color",
    "flex-grow",
    "max-width",
    "grid-column",
    "image-orientation",
    "animation-duration",
    "-webkit-columns",
    "-webkit-animation-delay",
    "-epub-text-emphasis",
    "flex-shrink",
    "text-rendering",
    "align-items",
    "border-collapse",
    "offset",
    "text-combine-upright",
    "-webkit-mask-position-x",
    "-webkit-mask-position-y",
    "outline-style",
    "-webkit-margin-bottom-collapse",
    "color-interpolation-filters",
    "font-variant",
    "-webkit-animation-fill-mode",
    "border-right",
    "visibility",
    "transform-box",
    "font-variant-caps",
    "-epub-text-emphasis-color",
    "-webkit-border-before-style",
    "resize",
    "-webkit-rtl-ordering",
    "-webkit-box-ordinal-group",
    "paint-order",
    "stroke-linecap",
    "animation-direction",
    "-webkit-font-feature-settings",
    "border-top-left-radius",
    "-webkit-column-width",
    "-webkit-box-align",
    "-webkit-padding-after",
    "column-width",
    "list-style",
    "-webkit-mask-repeat-y",
    "-webkit-margin-before-collapse",
    "stroke",
    "text-decoration-line",
    "-webkit-background-size",
    "-webkit-mask-repeat-x",
    "padding-bottom",
    "font-style",
    "-webkit-transition-delay",
    "background-repeat",
    "flex-basis",
    "border-image-slice",
    "-webkit-transform-origin",
    "scroll-boundary-behavior-x",
    "scroll-boundary-behavior-y",
    "vector-effect",
    "-webkit-animation-timing-function",
    "-webkit-border-after-style",
    "-webkit-perspective-origin-x",
    "-webkit-perspective-origin-y",
    "inline-size",
    "outline",
    "font-display",
    "-webkit-border-before",
    "border-image-source",
    "transition-duration",
    "scroll-padding-top",
    "order",
    "-webkit-box-orient",
    "counter-reset",
    "color-rendering",
    "flex-direction",
    "-webkit-text-stroke-width",
    "font-variant-numeric",
    "scroll-margin-block-end",
    "min-height",
    "scroll-padding-inline-start",
    "-webkit-mask-box-image",
    "left",
    "-webkit-mask",
    "-webkit-border-after-width",
    "stroke-width",
    "-webkit-box-decoration-break",
    "-webkit-mask-position",
    "background-origin",
    "-webkit-border-start-color",
    "font-stretch",
    "-webkit-background-clip",
    "scroll-margin-top",
    "-webkit-border-horizontal-spacing",
    "border-radius",
    "flex",
    "text-indent",
    "hyphens",
    "column-rule-width",
    "-webkit-margin-after",
    "-epub-caption-side",
    "break-after",
    "text-transform",
    "touch-action",
    "font-size",
    "-webkit-animation-name",
    "scroll-padding-inline",
    "offset-path",
    "scroll-margin",
    "padding-top",
    "scroll-snap-align",
    "-webkit-text-combine",
    "-webkit-flex-shrink",
    "rx",
    "ry",
    "content",
    "padding-right",
    "-webkit-transform",
    "marker-mid",
    "-webkit-min-logical-width",
    "clip-rule",
    "font-family",
    "scroll-snap-type",
    "text-decoration-skip-ink",
    "transition",
    "filter",
    "border-right-width",
    "-webkit-flex-direction",
    "-webkit-mask-composite",
    "mix-blend-mode",
    "color-interpolation",
    "border-top-style",
    "fill-opacity",
    "marker-start",
    "border-bottom-width",
    "-webkit-text-emphasis",
    "grid-area",
    "size",
    "background-clip",
    "-webkit-text-fill-color",
    "top",
    "-webkit-box-reflect",
    "border-width",
    "offset-anchor",
    "max-inline-size",
    "-webkit-column-rule-style",
    "-webkit-column-count",
    "animation-play-state",
    "padding",
    "dominant-baseline",
    "background-attachment",
    "-webkit-box-sizing",
    "-webkit-box-flex",
    "text-orientation",
    "background-position",
    "-webkit-border-start-width",
    "-epub-text-emphasis-style",
    "isolation",
    "-epub-text-orientation",
    "-webkit-border-bottom-right-radius",
    "r",
    "border-left-width",
    "grid-column-end",
    "background-blend-mode",
    "vertical-align",
    "clip",
    "grid-auto-rows",
    "offset-rotate",
    "margin-left",
    "animation-name",
    "text-decoration",
    "border",
    "-webkit-transition-timing-function",
    "margin-bottom",
    "unicode-range",
    "animation",
    "-webkit-shape-margin",
    "font-weight",
    "shape-margin",
    "mask-type",
    "scroll-padding",
    "min-inline-size",
    "object-position",
    "page-break-after",
    "-webkit-mask-clip",
    "white-space",
    "-webkit-border-after-color",
    "-webkit-max-logical-width",
    "-webkit-border-before-color",
    "font-kerning",
    "-epub-word-break",
    "clear",
    "animation-timing-function",
    "-webkit-border-radius",
    "scroll-padding-right",
    "-webkit-text-decorations-in-effect",
    "-webkit-animation-direction",
    "justify-self",
    "transition-timing-function",
    "scroll-snap-stop",
    "counter-increment",
    "-webkit-transform-style",
    "grid-auto-columns",
    "-webkit-align-content",
    "font",
    "flex-wrap",
    "grid-row-start",
    "list-style-image",
    "-webkit-tap-highlight-color",
    "-webkit-text-emphasis-color",
    "border-left",
    "-webkit-border-end-color",
    "columns",
    "box-shadow",
    "-webkit-flex-wrap",
    "align-self",
    "border-bottom",
    "border-spacing",
    "-webkit-column-span",
    "grid-row-end",
    "-webkit-border-end",
    "perspective-origin",
    "page-break-inside",
    "orphans",
    "-webkit-border-start-style",
    "scroll-behavior",
    "column-span",
    "-webkit-hyphenate-character",
    "column-fill",
    "tab-size",
    "contain",
    "x",
    "grid-row",
    "border-bottom-right-radius",
    "line-height",
    "stroke-linejoin",
    "text-align-last",
    "offset-position",
    "word-spacing",
    "transform-style",
    "-webkit-app-region",
    "-webkit-border-end-style",
    "-webkit-transform-origin-z",
    "-webkit-transform-origin-x",
    "-webkit-transform-origin-y",
    "background-repeat-x",
    "background-repeat-y",
    "border-bottom-color",
    "-webkit-ruby-position",
    "-webkit-logical-width",
    "text-justify",
    "scroll-margin-inline-start",
    "caption-side",
    "mask-source-type",
    "-webkit-mask-box-image-slice",
    "-webkit-border-image",
    "text-size-adjust",
    "-webkit-text-security",
    "-epub-writing-mode",
    "grid-template",
    "-webkit-mask-box-image-repeat",
    "-webkit-mask-repeat",
    "-webkit-justify-content",
    "baseline-shift",
    "border-image",
    "text-decoration-color",
    "color",
    "shape-image-threshold",
    "shape-rendering",
    "cy",
    "cx",
    "-webkit-user-modify",
    "offset-distance",
    "-webkit-border-bottom-left-radius",
    "speak",
    "border-bottom-left-radius",
    "-webkit-column-break-after",
    "-webkit-font-smoothing",
    "-webkit-max-logical-height",
    "-webkit-line-break",
    "fill-rule",
    "-webkit-margin-start",
    "min-width",
    "-epub-text-combine",
    "break-before",
    "caret-color",
    "empty-cells",
    "direction",
    "clip-path",
    "justify-content",
    "scroll-padding-block-end",
    "z-index",
    "background-position-y",
    "text-decoration-style",
    "grid-template-areas",
    "-webkit-min-logical-height",
    "font-size-adjust",
    "scroll-padding-block",
    "overflow-anchor",
    "cursor",
    "scroll-margin-block-start",
    "-webkit-mask-box-image-source",
    "margin",
    "-webkit-animation",
    "letter-spacing",
    "orientation",
    "will-change",
    "-webkit-highlight",
    "transform-origin",
    "font-variant-ligatures",
    "-webkit-animation-duration",
    "-webkit-mask-origin",
    "-webkit-clip-path",
    "word-break",
    "table-layout",
    "text-overflow",
    "-webkit-locale",
    "-webkit-flex",
    "grid-auto-flow",
    "border-top-right-radius",
    "border-image-outset",
    "place-items",
    "border-left-color",
    "font-variation-settings",
    "border-right-color",
    "min-zoom",
    "scroll-margin-inline",
    "-webkit-border-before-width",
    "backface-visibility",
    "background-image",
    "-webkit-transition-property",
    "writing-mode",
    "stroke-opacity",
    "-webkit-margin-collapse",
    "box-sizing",
    "margin-top",
    "column-rule-color",
    "y",
    "position",
    "scroll-margin-bottom",
    "list-style-position",
    "-webkit-box-pack",
    "scroll-padding-inline-end",
    "quotes",
    "border-top",
    "scroll-padding-left",
    "-webkit-transition",
    "-webkit-column-break-before",
    "lighting-color",
    "background-size",
    "-webkit-padding-before",
    "-webkit-border-top-left-radius",
    "flood-opacity",
    "line-height-step",
    "-webkit-mask-size",
    "text-align",
    "-webkit-filter",
    "word-wrap",
    "max-zoom",
    "grid",
    "background",
    "height",
    "grid-column-start",
    "animation-fill-mode",
    "rotate",
    "marker-end",
    "d",
    "justify-items",
    "zoom",
    "scroll-padding-block-start",
    "-webkit-margin-top-collapse",
    "page",
    "right",
    "user-select",
    "margin-right",
    "marker",
    "line-break",
    "-webkit-margin-end",
    "-webkit-transition-duration",
    "-webkit-writing-mode",
    "border-top-width",
    "bottom",
    "place-content",
    "-webkit-shape-image-threshold",
    "-webkit-user-drag",
    "-webkit-border-vertical-spacing",
    "-webkit-column-gap",
    "-webkit-opacity",
    "background-color",
    "column-gap",
    "shape-outside",
    "-webkit-padding-end",
    "-webkit-border-start",
    "animation-delay",
    "unicode-bidi",
    "text-shadow",
    "-webkit-box-direction",
    "image-rendering",
    "src",
    "gap",
    "grid-gap",
    "pointer-events",
    "border-image-width",
    "min-block-size",
    "transition-property",
    "-webkit-mask-image",
    "float",
    "max-height",
    "outline-offset",
    "-webkit-box-shadow",
    "overflow-wrap",
    "block-size",
    "transform",
    "place-self",
    "width",
    "stroke-miterlimit",
    "stop-opacity",
    "border-top-color",
    "translate",
    "object-fit",
    "-webkit-mask-box-image-width",
    "-webkit-background-origin",
    "-webkit-align-items",
    "transition-delay",
    "scroll-margin-left",
    "border-style",
    "animation-iteration-count",
    "-webkit-margin-after-collapse",
    "overflow",
    "user-zoom",
    "-webkit-border-top-right-radius",
    "grid-template-columns",
    "-webkit-align-self",
    "-webkit-perspective-origin",
    "column-rule-style",
    "display",
    "-webkit-column-rule-width",
    "border-color",
    "-webkit-flex-basis",
    "stroke-dashoffset",
    "-webkit-text-size-adjust",
    "scroll-boundary-behavior",
    "-webkit-text-stroke",
    "widows",
    "fill",
    "overflow-y",
    "overflow-x",
    "opacity",
    "-webkit-perspective",
    "-webkit-text-stroke-color",
    "scroll-margin-inline-end",
    "scale",
    "-webkit-text-orientation",
    "-webkit-mask-box-image-outset",
    "align-content",
    "-webkit-border-end-width",
    "border-bottom-style",
    "mask",
    "background-position-x",
    "-epub-text-transform",
    "stop-color",
    "stroke-dasharray",
    "-webkit-line-clamp",
    "margin-block-start",
    "margin-block-end",
    "margin-inline-start",
    "margin-inline-end",
    "padding-block-start",
    "padding-block-end",
    "padding-inline-start",
    "padding-inline-end",
    "border-block-start-width",
    "border-block-start-style",
    "border-block-start-color",
    "border-block-end-width",
    "border-block-end-style",
    "border-block-end-color",
    "border-inline-start-width",
    "border-inline-start-style",
    "border-inline-start-color",
    "border-inline-end-width",
    "border-inline-end-style",
    "border-inline-end-color",
    "border-block-start",
    "border-block-end",
    "border-inline-start",
    "border-inline-end",
    "margin-block",
    "margin-inline",
    "padding-block",
    "padding-inline",
    "border-block-width",
    "border-block-style",
    "border-block-color",
    "border-inline-width",
    "border-inline-style",
    "border-inline-color",
    "border-block",
    "border-inline",
    "inset-block-start",
    "inset-block-end",
    "inset-block",
    "inset-inline-start",
    "inset-inline-end",
    "inset-inline",
    "inset",
    "overflow-block",
    "overflow-inline",
    "forced-color-adjust",
    "overscroll-behavior-inline",
    "overscroll-behavior-block",
    "overscroll-behavior-x",
    "overscroll-behavior-y",
    "INVALID_PROPERTY",
};

Converter::Converter() = default;

std::string Converter::Convert(const StyleSheet& style_sheet_message) {
  Reset();
  Visit(style_sheet_message);
  return string_;
}

void Converter::Visit(const Unicode& unicode) {
  string_ += "\\";
  string_ += static_cast<char>(unicode.ascii_value_1());

  if (unicode.has_ascii_value_2())
    string_ += static_cast<char>(unicode.ascii_value_2());
  if (unicode.has_ascii_value_3())
    string_ += static_cast<char>(unicode.ascii_value_3());
  if (unicode.has_ascii_value_4())
    string_ += static_cast<char>(unicode.ascii_value_4());
  if (unicode.has_ascii_value_5())
    string_ += static_cast<char>(unicode.ascii_value_5());
  if (unicode.has_ascii_value_6())
    string_ += static_cast<char>(unicode.ascii_value_6());

  if (unicode.has_unrepeated_w())
    Visit(unicode.unrepeated_w());
}

void Converter::Visit(const Escape& escape) {
  if (escape.has_ascii_value()) {
    string_ += "\\";
    string_ += static_cast<char>(escape.ascii_value());
  } else if (escape.has_unicode()) {
    Visit(escape.unicode());
  }
}

void Converter::Visit(const Nmstart& nmstart) {
  if (nmstart.has_ascii_value())
    string_ += static_cast<char>(nmstart.ascii_value());
  else if (nmstart.has_escape())
    Visit(nmstart.escape());
}

void Converter::Visit(const Nmchar& nmchar) {
  if (nmchar.has_ascii_value())
    string_ += static_cast<char>(nmchar.ascii_value());
  else if (nmchar.has_escape())
    Visit(nmchar.escape());
}

void Converter::Visit(const String& string) {
  bool use_single_quotes = string.use_single_quotes();
  if (use_single_quotes)
    string_ += "'";
  else
    string_ += "\"";

  for (auto& string_char_quote : string.string_char_quotes())
    Visit(string_char_quote, use_single_quotes);

  if (use_single_quotes)
    string_ += "'";
  else
    string_ += "\"";
}

void Converter::Visit(const StringCharOrQuote& string_char_quote,
                      bool using_single_quote) {
  if (string_char_quote.has_string_char()) {
    Visit(string_char_quote.string_char());
  } else if (string_char_quote.quote_char()) {
    if (using_single_quote)
      string_ += "\"";
    else
      string_ += "'";
  }
}

void Converter::Visit(const StringChar& string_char) {
  if (string_char.has_url_char())
    Visit(string_char.url_char());
  else if (string_char.has_space())
    string_ += " ";
  else if (string_char.has_nl())
    Visit(string_char.nl());
}

void Converter::Visit(const Ident& ident) {
  if (ident.starting_minus())
    string_ += "-";
  Visit(ident.nmstart());
  for (auto& nmchar : ident.nmchars())
    Visit(nmchar);
}

void Converter::Visit(const Num& num) {
  if (num.has_float_value())
    string_ += std::to_string(num.float_value());
  else
    string_ += std::to_string(num.signed_int_value());
}

void Converter::Visit(const UrlChar& url_char) {
  string_ += static_cast<char>(url_char.ascii_value());
}

// TODO(metzman): implement W
void Converter::Visit(const UnrepeatedW& unrepeated_w) {
  string_ += static_cast<char>(unrepeated_w.ascii_value());
}

void Converter::Visit(const Nl& nl) {
  string_ += "\\";
  if (nl.newline_kind() == Nl::CR_LF)
    string_ += "\r\n";
  else  // Otherwise newline_kind is the ascii value of the char we want.
    string_ += static_cast<char>(nl.newline_kind());
}

void Converter::Visit(const Length& length) {
  Visit(length.num());
  if (length.unit() == Length::PX)
    string_ += "px";
  else if (length.unit() == Length::CM)
    string_ += "cm";
  else if (length.unit() == Length::MM)
    string_ += "mm";
  else if (length.unit() == Length::IN)
    string_ += "in";
  else if (length.unit() == Length::PT)
    string_ += "pt";
  else if (length.unit() == Length::PC)
    string_ += "pc";
  else
    NOTREACHED();
}

void Converter::Visit(const Angle& angle) {
  Visit(angle.num());
  if (angle.unit() == Angle::DEG)
    string_ += "deg";
  else if (angle.unit() == Angle::RAD)
    string_ += "rad";
  else if (angle.unit() == Angle::GRAD)
    string_ += "grad";
  else
    NOTREACHED();
}

void Converter::Visit(const Time& time) {
  Visit(time.num());
  if (time.unit() == Time::MS)
    string_ += "ms";
  else if (time.unit() == Time::S)
    string_ += "s";
  else
    NOTREACHED();
}

void Converter::Visit(const Freq& freq) {
  Visit(freq.num());
  // Hack around really dumb build bug
  if (freq.unit() == Freq::_HZ)
    string_ += "Hz";
  else if (freq.unit() == Freq::KHZ)
    string_ += "kHz";
  else
    NOTREACHED();
}

void Converter::Visit(const Uri& uri) {
  string_ += "url(\"chrome://communicator/skin/\");";
}

void Converter::Visit(const FunctionToken& function_token) {
  Visit(function_token.ident());
  string_ += "(";
}

void Converter::Visit(const StyleSheet& style_sheet) {
  if (style_sheet.has_charset_declaration())
    Visit(style_sheet.charset_declaration());
  for (auto& import : style_sheet.imports())
    Visit(import);
  for (auto& _namespace : style_sheet.namespaces())
    Visit(_namespace);
  for (auto& nested_at_rule : style_sheet.nested_at_rules())
    Visit(nested_at_rule);
}

void Converter::Visit(const ViewportValue& viewport_value) {
  if (viewport_value.has_length())
    Visit(viewport_value.length());
  else if (viewport_value.has_num())
    Visit(viewport_value.num());
  else  // Default value.
    AppendTableValue(viewport_value.value_id(), kViewportValueLookupTable);
}

void Converter::Visit(const Viewport& viewport) {
  string_ += " @viewport {";
  for (auto& property_and_value : viewport.properties_and_values())
    AppendPropertyAndValue(property_and_value, kViewportPropertyLookupTable);
  string_ += " } ";
}

void Converter::Visit(const CharsetDeclaration& charset_declaration) {
  string_ += "@charset ";  // CHARSET_SYM
  string_ += "\"";
  AppendTableValue(charset_declaration.encoding_id(), kEncodingLookupTable);
  string_ += "\"; ";
}

void Converter::Visit(const AtRuleOrRulesets& at_rule_or_rulesets, int depth) {
  Visit(at_rule_or_rulesets.first(), depth);
  for (auto& later : at_rule_or_rulesets.laters())
    Visit(later, depth);
}

void Converter::Visit(const AtRuleOrRuleset& at_rule_or_ruleset, int depth) {
  if (at_rule_or_ruleset.has_at_rule())
    Visit(at_rule_or_ruleset.at_rule(), depth);
  else  // Default.
    Visit(at_rule_or_ruleset.ruleset());
}

void Converter::Visit(const NestedAtRule& nested_at_rule, int depth) {
  if (++depth > kAtRuleDepthLimit)
    return;

  if (nested_at_rule.has_ruleset())
    Visit(nested_at_rule.ruleset());
  else if (nested_at_rule.has_media())
    Visit(nested_at_rule.media());
  else if (nested_at_rule.has_viewport())
    Visit(nested_at_rule.viewport());
  else if (nested_at_rule.has_supports_rule())
    Visit(nested_at_rule.supports_rule(), depth);
  // Else apppend nothing.
  // TODO(metzman): Support pages and font-faces.
}

void Converter::Visit(const SupportsRule& supports_rule, int depth) {
  string_ += "@supports ";
  Visit(supports_rule.supports_condition(), depth);
  string_ += " { ";
  for (auto& at_rule_or_ruleset : supports_rule.at_rule_or_rulesets())
    Visit(at_rule_or_ruleset, depth);
  string_ += " } ";
}

void Converter::AppendBinarySupportsCondition(
    const BinarySupportsCondition& binary_condition,
    std::string binary_operator,
    int depth) {
  Visit(binary_condition.condition_1(), depth);
  string_ += " " + binary_operator + " ";
  Visit(binary_condition.condition_2(), depth);
}

void Converter::Visit(const SupportsCondition& supports_condition, int depth) {
  bool under_depth_limit = ++depth <= kSupportsConditionDepthLimit;

  if (supports_condition.not_condition())
    string_ += " not ";

  string_ += "(";

  if (under_depth_limit && supports_condition.has_and_supports_condition()) {
    AppendBinarySupportsCondition(supports_condition.or_supports_condition(),
                                  "and", depth);
  } else if (under_depth_limit &&
             supports_condition.has_or_supports_condition()) {
    AppendBinarySupportsCondition(supports_condition.or_supports_condition(),
                                  "or", depth);
  } else {
    // Use the required property_and_value field if the or_supports_condition
    // and and_supports_condition are unset or if we have reached the depth
    // limit and don't want another nested condition.
    Visit(supports_condition.property_and_value());
  }

  string_ += ")";
}

void Converter::Visit(const Import& import) {
  string_ += "@import ";
  AppendTableValue(import.src_id(), kImportLookupTable);
  string_ += " ";
  if (import.has_media_query_list())
    Visit(import.media_query_list());
  string_ += "; ";
}

void Converter::Visit(const MediaQueryList& media_query_list) {
  bool first = true;
  for (auto& media_query : media_query_list.media_queries()) {
    if (first)
      first = false;
    else
      string_ += ", ";
    Visit(media_query);
  }
}

void Converter::Visit(const MediaQuery& media_query) {
  if (media_query.has_media_query_part_two())
    Visit(media_query.media_query_part_two());
  else
    Visit(media_query.media_condition());
}

void Converter::Visit(const MediaQueryPartTwo& media_query_part_two) {
  if (media_query_part_two.has_not_or_only()) {
    if (media_query_part_two.not_or_only() == MediaQueryPartTwo::NOT)
      string_ += " not ";
    else
      string_ += " only ";
  }
  Visit(media_query_part_two.media_type());
  if (media_query_part_two.has_media_condition_without_or()) {
    string_ += " and ";
    Visit(media_query_part_two.media_condition_without_or());
  }
}

void Converter::Visit(const MediaCondition& media_condition) {
  if (media_condition.has_media_not()) {
    Visit(media_condition.media_not());
  } else if (media_condition.has_media_or()) {
    Visit(media_condition.media_or());
  } else if (media_condition.has_media_in_parens()) {
    Visit(media_condition.media_in_parens());
  } else {
    Visit(media_condition.media_and());
  }
}

void Converter::Visit(const MediaConditionWithoutOr& media_condition) {
  if (media_condition.has_media_and()) {
    Visit(media_condition.media_and());
  } else if (media_condition.has_media_in_parens()) {
    Visit(media_condition.media_in_parens());
  } else {
    Visit(media_condition.media_not());
  }
}

void Converter::Visit(const MediaType& media_type) {
  AppendTableValue(media_type.value_id(), kMediaTypeLookupTable);
}

void Converter::Visit(const MediaNot& media_not) {
  string_ += " not ";
  Visit(media_not.media_in_parens());
}

void Converter::Visit(const MediaAnd& media_and) {
  Visit(media_and.first_media_in_parens());
  string_ += " and ";
  Visit(media_and.second_media_in_parens());
  for (auto& media_in_parens : media_and.media_in_parens_list()) {
    string_ += " and ";
    Visit(media_in_parens);
  }
}

void Converter::Visit(const MediaOr& media_or) {
  Visit(media_or.first_media_in_parens());
  string_ += " or ";
  Visit(media_or.second_media_in_parens());
  for (auto& media_in_parens : media_or.media_in_parens_list()) {
    string_ += " or ";
    Visit(media_in_parens);
  }
}

void Converter::Visit(const MediaInParens& media_in_parens) {
  if (media_in_parens.has_media_condition()) {
    string_ += " (";
    Visit(media_in_parens.media_condition());
    string_ += " )";
  } else if (media_in_parens.has_media_feature()) {
    Visit(media_in_parens.media_feature());
  }
}

void Converter::Visit(const MediaFeature& media_feature) {
  string_ += "(";
  if (media_feature.has_mf_bool()) {
    Visit(media_feature.mf_bool());
  } else if (media_feature.has_mf_plain()) {
    AppendPropertyAndValue(media_feature.mf_plain(), kMfNameLookupTable, false);
  }
  string_ += ")";
}

void Converter::Visit(const MfBool& mf_bool) {
  Visit(mf_bool.mf_name());
}

void Converter::Visit(const MfName& mf_name) {
  AppendTableValue(mf_name.id(), kMfNameLookupTable);
}

void Converter::Visit(const MfValue& mf_value) {
  if (mf_value.has_length()) {
    Visit(mf_value.length());
  } else if (mf_value.has_ident()) {
    Visit(mf_value.ident());
  } else {
    Visit(mf_value.num());
  }
}

void Converter::Visit(const Namespace& _namespace) {
  string_ += "@namespace ";
  if (_namespace.has_namespace_prefix())
    Visit(_namespace.namespace_prefix());
  if (_namespace.has_string())
    Visit(_namespace.string());
  if (_namespace.has_uri())
    Visit(_namespace.uri());

  string_ += "; ";
}

void Converter::Visit(const NamespacePrefix& namespace_prefix) {
  Visit(namespace_prefix.ident());
}

void Converter::Visit(const Media& media) {
  // MEDIA_SYM S*
  string_ += "@media ";  // "@media" {return MEDIA_SYM;}

  Visit(media.media_query_list());
  string_ += " { ";
  for (auto& ruleset : media.rulesets())
    Visit(ruleset);
  string_ += " } ";
}

void Converter::Visit(const Page& page) {
  // PAGE_SYM
  string_ += "@page ";  // PAGE_SYM
  if (page.has_ident())
    Visit(page.ident());
  if (page.has_pseudo_page())
    Visit(page.pseudo_page());
  string_ += " { ";
  Visit(page.declaration_list());
  string_ += " } ";
}

void Converter::Visit(const PseudoPage& pseudo_page) {
  string_ += ":";
  Visit(pseudo_page.ident());
}

void Converter::Visit(const DeclarationList& declaration_list) {
  Visit(declaration_list.first_declaration());
  for (auto& declaration : declaration_list.later_declarations()) {
    Visit(declaration);
    string_ += "; ";
  }
}

void Converter::Visit(const FontFace& font_face) {
  string_ += "@font-face";
  string_ += "{";
  // Visit(font_face.declaration_list());
  string_ += "}";
}

void Converter::Visit(const Operator& _operator) {
  if (_operator.has_ascii_value())
    string_ += static_cast<char>(_operator.ascii_value());
}

void Converter::Visit(const UnaryOperator& unary_operator) {
  string_ += static_cast<char>(unary_operator.ascii_value());
}

void Converter::Visit(const Property& property) {
  AppendTableValue(property.name_id(), kPropertyLookupTable);
}

void Converter::Visit(const Ruleset& ruleset) {
  Visit(ruleset.selector_list());
  string_ += " {";
  Visit(ruleset.declaration_list());
  string_ += "} ";
}

void Converter::Visit(const SelectorList& selector_list) {
  Visit(selector_list.first_selector(), true);
  for (auto& selector : selector_list.later_selectors()) {
    Visit(selector, false);
  }
  string_ += " ";
}

// Also visits Attr
void Converter::Visit(const Selector& selector, bool is_first) {
  if (!is_first) {
    string_ += " ";
    if (selector.combinator() != Combinator::NONE) {
      string_ += static_cast<char>(selector.combinator());
      string_ += " ";
    }
  }
  if (selector.type() == Selector::ELEMENT) {
    string_ += "a";
  } else if (selector.type() == Selector::CLASS) {
    string_ += ".classname";
  } else if (selector.type() == Selector::ID) {
    string_ += "#idname";
  } else if (selector.type() == Selector::UNIVERSAL) {
    string_ += "*";
  } else if (selector.type() == Selector::ATTR) {
    std::string val1 = "href";
    std::string val2 = ".org";
    string_ += "a[" + val1;
    if (selector.attr().type() != Attr::NONE) {
      string_ += " ";
      string_ += static_cast<char>(selector.attr().type());
      string_ += +"= " + val2;
    }
    if (selector.attr().attr_i())
      string_ += " i";
    string_ += "]";
  }
  if (selector.has_pseudo_value_id()) {
    string_ += ":";
    if (selector.pseudo_type() == PseudoType::ELEMENT)
      string_ += ":";
    AppendTableValue(selector.pseudo_value_id(), kPseudoLookupTable);
  }
}

void Converter::Visit(const Declaration& declaration) {
  if (declaration.has_property_and_value())
    Visit(declaration.property_and_value());
  // else empty
}

void Converter::Visit(const PropertyAndValue& property_and_value) {
  Visit(property_and_value.property());
  string_ += " : ";
  int value_id = 0;
  if (property_and_value.has_value_id())
    value_id = property_and_value.value_id();
  Visit(property_and_value.expr(), value_id);
  if (property_and_value.has_prio())
    string_ += " !important ";
}

void Converter::Visit(const Expr& expr, int declaration_value_id) {
  if (!declaration_value_id)
    Visit(expr.term());
  else
    AppendTableValue(declaration_value_id, kValueLookupTable);
  for (auto& operator_term : expr.operator_terms())
    Visit(operator_term);
}

void Converter::Visit(const OperatorTerm& operator_term) {
  Visit(operator_term._operator());
  Visit(operator_term.term());
}

void Converter::Visit(const Term& term) {
  if (term.has_unary_operator())
    Visit(term.unary_operator());

  if (term.has_term_part())
    Visit(term.term_part());
  else if (term.has_string())
    Visit(term.string());

  if (term.has_ident())
    Visit(term.ident());
  if (term.has_uri())
    Visit(term.uri());
  if (term.has_hexcolor())
    Visit(term.hexcolor());
}

void Converter::Visit(const TermPart& term_part) {
  if (term_part.has_number())
    Visit(term_part.number());
  // S* | PERCENTAGE
  if (term_part.has_percentage()) {
    Visit(term_part.percentage());
    string_ += "%";
  }
  // S* | LENGTH
  if (term_part.has_length())
    Visit(term_part.length());
  // S* | EMS
  if (term_part.has_ems()) {
    Visit(term_part.ems());
    string_ += "em";
  }
  // S* | EXS
  if (term_part.has_exs()) {
    Visit(term_part.exs());
    string_ += "ex";
  }
  // S* | Angle
  if (term_part.has_angle())
    Visit(term_part.angle());
  // S* | TIME
  if (term_part.has_time())
    Visit(term_part.time());
  // S* | FREQ
  if (term_part.has_freq())
    Visit(term_part.freq());
  // S* | function
  if (term_part.has_function()) {
    Visit(term_part.function());
  }
}

void Converter::Visit(const Function& function) {
  Visit(function.function_token());
  Visit(function.expr());
  string_ += ")";
}

void Converter::Visit(const Hexcolor& hexcolor) {
  string_ += "#";
  Visit(hexcolor.first_three());
  if (hexcolor.has_last_three())
    Visit(hexcolor.last_three());
}

void Converter::Visit(const HexcolorThree& hexcolor_three) {
  string_ += static_cast<char>(hexcolor_three.ascii_value_1());
  string_ += static_cast<char>(hexcolor_three.ascii_value_2());
  string_ += static_cast<char>(hexcolor_three.ascii_value_3());
}

void Converter::Reset() {
  string_.clear();
}

template <size_t TableSize>
void Converter::AppendTableValue(int id,
                                 const std::string (&lookup_table)[TableSize]) {
  CHECK(id > 0 && static_cast<size_t>(id) < TableSize);
  string_ += lookup_table[id];
}

template <class T, size_t TableSize>
void Converter::AppendPropertyAndValue(
    T property_and_value,
    const std::string (&lookup_table)[TableSize],
    bool append_semicolon) {
  AppendTableValue(property_and_value.property().id(), lookup_table);
  string_ += " : ";
  Visit(property_and_value.value());
  if (append_semicolon)
    string_ += "; ";
}
}  // namespace css_proto_converter
