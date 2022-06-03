# CSS Typed OM

[Rendered](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/core/css/cssom/README.md)

The `Source/core/css/cssom` directory contains the implementation of [CSS Typed OM](https://drafts.css-houdini.org/css-typed-om).

# Appendix

## Supported properties

Blink ships CSS Typed OM for a subset of properties. Any property that is not
supported will be represented as a base CSSStyleValue, which can be upgraded
to more powerful types once they are supported. Expect Blink to support more
properties over time as they get spec'd and tested.

List of supported properties as of 1 March 2018:

- animation-direction
- backface-visibility
- background-color
- background-image
- border-bottom-color
- border-bottom-style
- border-bottom-width
- border-collapse
- border-image-source
- border-left-color
- border-left-style
- border-left-width
- border-right-color
- border-right-style
- border-right-width
- border-top-color
- border-top-style
- border-top-width
- bottom
- box-sizing
- caret-color
- clear
- color
- column-rule-color
- direction
- display
- empty-cells
- float
- font-style
- font-weight
- height
- left
- line-height
- list-style-image
- list-style-position
- margin-bottom
- margin-left
- margin-right
- margin-top
- object-position
- opacity
- outline-color
- outline-style
- overflow-anchor
- overflow-x
- overflow-y
- padding-bottom
- padding-left
- padding-right
- padding-top
- position
- resize
- right
- shape-outside
- text-align
- text-decoration-color
- text-decoration-style
- text-transform
- top
- transform
- transition-duration
- vertical-align
- visibility
- white-space
- width
