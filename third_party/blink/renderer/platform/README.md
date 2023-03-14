# Blink Renderer Platform

The `renderer/platform/` directory contains lower-level, self-contained
abstractions that `core/` and `modules/` can depend on.

See [renderer/README.md](../README.md) for more about the relationship of
`platform/` to `core/` and `modules/`.

Here is a non-exhaustive list of some major things in `renderer/platform/`:

* [Runtime Enabled Features](RuntimeEnabledFeatures.md) are runtime flags for
  new web-exposed features
* [`bindings/`](bindings/README.md) contains reusable components for the V8-DOM
  bindings layer
* `exported/` implements some classes in the [Blink Public
  API](../../public/README.md) which are declared in
  [`public/platform/`](../../public/platform/), including
  [blink::Platform](../../public/platform/platform.h) which initializes Blink
* [`fonts/`](fonts/README.md) and `text/` contain Blink's font and text stack
* [`graphics/`](graphics/README.md) contains graphics support code including
  the [Blink compositing algorithm](graphics/compositing/README.md)
* [`heap/`](heap/README.md) contains the Blink GC system (a.k.a. Oilpan)
* [`loader/`](loader/README.md) contains functionality for loading resources
  from the network
* [`scheduler/`](scheduler/README.md) contains the Blink Scheduler which
  coordinates task execution in renderer processes
* [`widget/`](widget/) handles input and compositing;
  [WidgetBase](widget/widget_base.h) owns
  [LayerTreeView](widget/compositing/layer_tree_view.h) which wraps
  [`cc/`](../../../../cc/README.md) (the renderer compositor)
* [`wtf/`](wtf/README.md) (Web Template Framework) is a library of containers
  and other basic functionalities
