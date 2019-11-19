# CSS Paint API

This directory contains the implementation of the CSS Paint API.

See [CSS Paint API](https://drafts.css-houdini.org/css-paint-api/) for the web exposed APIs this
implements.

See [Explainer](https://github.com/w3c/css-houdini-drafts/blob/master/css-paint-api/EXPLAINER.md)
of this feature, as well as [Samples](https://github.com/GoogleChromeLabs/houdini-samples/tree/master/paint-worklet).

## Workflow

Historically the CSS Paint API (PaintWorklet) implementation ran on the main
thread, but is currently being optimized to run on the compositor thread. We
will use an example to show the workflow of both cases.

Here is a simple example of using PaintWorklet to draw something on the screen.

<style>
#demo {
  background-image: paint(foo);
  width: 200px;
  height: 200px;
}
</style>
<script>
registerPaint('foo', class {
  paint(ctx, size) {
    ctx.fillStyle = 'green';
    ctx.fillRect(0, 0, size.width, size.height);
  }
});
</script>

In our implementation, there is one [PaintWorklet](paint_worklet.h) instance
created from the frame. There are two
[PaintWorkletGlobalScope](paint_worklet_global_scope.h) created to enforce
stateless. The number of global scopes can be arbitrary, and our implementation
chose two.

During `PaintWorkletGlobalScope#registerPaint`, the Javascript inside the paint
function is turned into a V8 paint callback. We randomly choose one of the two
global scopes to execute the callback. The execution of the callback
produces a PaintRecord, which contains a set of skia draw commands. The V8 paint
callback is executed on a shared V8 isolate.

### Main thread workflow

During the main thread paint, the `PaintWorklet::Paint` is called, which
executes the V8 paint callback synchronously. A PaintRecord is produced and
passed to the compositor thread to raster.

### Off main thread workflow

During the main thread paint, a
[PaintWorkletInput](../../core/css/cssom/paint_worklet_input.h) is created and
passed to the compositor. The PaintWorkletInput contains all necessary
information for the compositor to run the V8 paint callback. The compositor
thread asynchronously dispatches the V8 paint callback to a Worklet thread.
The V8 paint callback is then executed on the Worklet thread and a PaintRecord is
given back to the compositor thread. The compositor thread then rasters the
PaintRecord.

## Implementation

### [CSSPaintDefinition](css_paint_definition.h)

Represents a class registered by the author through `PaintWorkletGlobalScope#registerPaint`.
Specifically this class holds onto the javascript constructor and paint functions of the class via
persistent handles. This class keeps these functions alive so they don't get garbage collected.

The `CSSPaintDefinition` also holds onto an instance of the paint class via a persistent handle. This
instance is lazily created upon first use. If the constructor throws for some reason the constructor
is marked as invalid and will always produce invalid images.

The `PaintWorkletGlobalScope` has a map of paint `name` to `CSSPaintDefinition`.

### [CSSPaintImageGenerator][generator] and [CSSPaintImageGeneratorImpl][generator-impl]

`CSSPaintImageGenerator` represents the interface from which the `CSSPaintValue` can generate
`Image`s. This is done via the `CSSPaintImageGenerator#paint` method. Each `CSSPaintValue` owns a
separate instance of `CSSPaintImageGenerator`.

`CSSPaintImageGeneratorImpl` is the implementation which lives in `modules/csspaint`. (We have this
interface / implementation split as `core/` cannot depend on `modules/`).

When created the generator will access its paint worklet and lookup it's corresponding
`CSSPaintDefinition` via `PaintWorkletGlobalScope#findDefinition`.

If the paint worklet does not have a `CSSPaintDefinition` matching the paint `name` the
`CSSPaintImageGeneratorImpl` is placed in a "pending" map. Once a paint class with `name` is
registered the generator is notified so it can invalidate an display the correct image.

[generator]: ../../core/css/css_paint_image_generator.h
[generator-impl]: css_paint_image_generator_impl.h
[paint-value]: ../../core/css/css_paint_value.h

### Generating a [PaintGeneratedImage](../../platform/graphics/paint_generated_image.h)

`PaintGeneratedImage` is a `Image` which just paints a single `PaintRecord`.

A `CSSPaintValue` can generate an image from the method `CSSPaintImageGenerator#paint`. This method
calls through to `CSSPaintDefinition#paint` which actually invokes the javascript paint method.
This method returns the `PaintGeneratedImage`.

### Style Invalidation

The `CSSPaintDefinition` keeps a list of both native and custom properties it will invalidate on.
During style invalidation `ComputedStyle` checks if it has any `CSSPaintValue`s, and if any of their
properties have changed; if so it will invalidate paint for that `ComputedStyle`.

If the `CSSPaintValue` doesn't have a corresponding `CSSPaintDefinition` yet, it doesn't invalidate
paint.

## Testing

Tests live [here](../../../web_tests/http/tests/csspaint/) and
[here](../../../web_tests/external/wpt/css/css-paint-api/).

