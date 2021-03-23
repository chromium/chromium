# Mapping of spec concepts to code

Blink is Chromium's implementation of the open web platform. This document
attempts to map terms and concepts found in the specification of the open web
platform to classes and files found in Blink's source tree.

[TOC]

## HTML

Concepts found in the [HTML spec](https://html.spec.whatwg.org/).

### [browsing context](https://html.spec.whatwg.org/C/#browsing-context)

A browsing context corresponds to the [Frame] interface where the main
implementation is [LocalFrame].

[Frame]: https://cs.chromium.org/src/third_party/blink/renderer/core/frame/frame.h
[LocalFrame]: https://cs.chromium.org/src/third_party/blink/renderer/core/frame/local_frame.h

### [origins](https://html.spec.whatwg.org/C/#concept-origin)

An origin corresponds to the [SecurityOrigin]. You can test for [same-origin]
using `SecurityOrigin::IsSameOriginWith` and for [same-origin domain] using
`SecurityOrigin::IsSameOriginDomainWith`. You can check for [same-site] using
`SecurityOrigin::IsSameSiteWith`, though "origin" should generally be used in
favor of "site" for any security decisions.

[SecurityOrigin]: https://cs.chromium.org/src/third_party/blink/renderer/platform/weborigin/security_origin.h
[same-origin]: https://html.spec.whatwg.org/C/#same-origin
[same-origin domain]: https://html.spec.whatwg.org/C/#same-origin-domain
[same-site]: https://html.spec.whatwg.org/C/#same-site


### [Window object](https://html.spec.whatwg.org/C/#window)

A Window object corresponds to the [DOMWindow] interface where the main
implementation is [LocalDOMWindow].

[DOMWindow]: https://cs.chromium.org/src/third_party/blink/renderer/core/frame/dom_window.h
[LocalDOMWindow]: https://cs.chromium.org/src/third_party/blink/renderer/core/frame/local_dom_window.h

### [WindowProxy](https://html.spec.whatwg.org/C/#windowproxy)

The WindowProxy is part of the bindings implemented by the
[WindowProxy class](https://cs.chromium.org/Source/bindings/core/v8/WindowProxy.h).

### [canvas](https://html.spec.whatwg.org/C/#the-canvas-element)

An HTML element into which drawing can be performed imperatively via
JavaScript. Multiple
[context types](https://html.spec.whatwg.org/C/#dom-canvas-getcontext)
are supported for different use cases.

The main element's sources are in [HTMLCanvasElement]. Contexts are implemented
via modules. The top-level module is [HTMLCanvasElementModule].

[HTMLCanvasElement]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/html/html_canvas_element.h
[HTMLCanvasElementModule]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/canvas/html_canvas_element_module.h


The [2D canvas context] is implemented in [modules/canvas2d].

[2D canvas context]: https://html.spec.whatwg.org/C/#canvasrenderingcontext2d
[modules/canvas2d]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/canvas2d/


The [WebGL 1.0] and [WebGL 2.0] contexts ([WebGL Github repo]) are implemented
in [modules/webgl].

[WebGL 1.0]: https://www.khronos.org/registry/webgl/specs/latest/1.0/
[WebGL 2.0]: https://www.khronos.org/registry/webgl/specs/latest/2.0/
[WebGL Github repo]: https://github.com/KhronosGroup/WebGL
[modules/webgl]: https://cs.chromium.org/chromium/src/third_party/blink/renderer/modules/webgl/
