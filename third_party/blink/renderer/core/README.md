# Blink Renderer Core

This directory implements core rendering aspects of the Web Platform.

See [renderer/README.md](../README.md) for the relationship of `core/` to
`modules/` and `platform/`.

See [How Blink Works](http://bit.ly/how-blink-works) for an introduction to
Blink's code architecture and directory structure.

See [Life of a Pixel](http://bit.ly/lifeofapixel) for an end-to-end tour of
the rendering pipeline.

The public mailing list is [rendering-core-dev](https://groups.google.com/a/chromium.org/g/rendering-core-dev).

Core rendering encompasses four key stages:

* [DOM](dom/README.md)
* [Style](css/README.md)
* [Layout](layout/README.md)
* [Paint](paint/README.md)

Other aspects of rendering are implemented outside of `core/`, such as
[compositing](../platform/graphics/compositing/README.md) and
[accessibility](../modules/accessibility/).

The `core/` directory includes concrete implementations of the classes in the
[Blink Public API](../../public/README.md), such as `WebLocalFrameImpl`.
The public API is used by the [Content module](../../../../content/README.md).

The output of core rendering is a `PaintArtifact` (see [platform paint
README](../platform/graphics/paint/README.md)) which is used to produce a
list of layers for the [renderer compositor](../../../../cc/README.md).
