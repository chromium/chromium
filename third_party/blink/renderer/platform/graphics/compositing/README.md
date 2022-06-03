# `Source/platform/graphics/compositing`

This directory contains the implementation of the "Blink compositing algorithm".

This code is owned by the [paint team][paint-team-site].
[paint-team-site]: https://www.chromium.org/teams/paint-team

This document explains the CAP world as it develops, not the SPv1 world it
replaces.

## Blink compositing algorithm

Design document: goo.gl/6xP8Oe

Inputs: `PaintArtifact`
Outputs: List of `cc::Layer` objects and `cc::PropertyTree`'s.

The algorithm walks through the list of `PaintChunks` in the `PaintArtifact`,
allocating new `cc::Layers` if the `PaintChunk` cannot merge into an existing
`cc::Layer`. The reasons why it would not be able to do so are:

1. The `PaintChunk` requires a foreign layer (see below)

2. The `PaintChunk` cannot merge with any existing layer, due incompatible
direct compositing reasons on its `PropertyTreeState`.

3. The `PaintChunk` overlaps with an earlier `cc::Layer` that it can't merge with
due to reason 2, and there is no later-drawn `cc::Layer` for which reasons 1 and
2 do not apply.

In the worst case, this algorithm has an O(n^2) running time, where n is the
number of `PaintChunks`.

All property tree nodes referred to by any `PaintChunk` are currently copied
into their equivalent `cc::PropertyTree` node, regardless of whether they are
required by the above.

### Flattening property tree nodes

When `PaintChunks` can merge into an existing `cc::Layer`, they may have
different `PropertyTreeState`s than the `PropertyTreeState` of the `cc::Layer`.
If so, we need to *flatten* down the nodes that are different between the
`PropertyTreeState` of the `PaintChunk` and the `cc::Layer`. This is done by
emitting paired display items to adjust the `PaintChunk`s property tree state
to the current state when the `PaintChunk` is consumed. See:
[`ConversionContext::Convert`](compositing/PaintChunksToCcLayer.cpp).

### Foreign layers

Some `PaintChunk` content requires a foreign layer, meaning a layer that is
managed outside of the scope of this code. Examples are composited video, a
and 2D/3D (WebGL) canvas.

### Raster invalidations

Any raster invalidates on a `PaintChunk` are also mapped to the space of the
backing `cc::Layer`.
