# Compute Pressure API

This directory contains the browser-side implementation of the
[Compute Pressure API](https://github.com/wicg/compute-pressure/).

## Code map

The system is made up of the following components.

`blink::mojom::ComputePressureHost`, defined in Blink, is the interface between
the renderer and the browser sides of the API implementation.

`content::ComputePressureManager` is the top-level class for the browser-side
implementation. Each instance handles the Compute Pressure API needs for a user
profile. The class is responsible for coordinating between the objects that
serve mojo requests from renderers, and the objects that collect compute
pressure information from the underlying operating system.

`content::ComputePressureHost` serves all the mojo connections from renderers
related to an origin. Each instance is owned by a `ComputePressureManager`,
which is responsible for creating and destorying instances as needed to meet
renderer requests.

`content::ComputePressureSampler` drives measuring the device's compute pressure
state. The class is responsible for invoking platform-specific measurement code
at regular intervals, and for straddling between sequences to meet the
platform-specific code's requirements.

`content::CpuProbe` is an abstract base class that interfaces between
`ComputePressureSampler` and platform-specific code that retrieves the compute
pressure state from the operating system. This interface is also a dependency
injection point for tests.

`content::ComputePressureSample` represents the device's compute pressure state.
This information is collected by `CpuProbe` and bubbled up by
`ComputePressureSampler` to `ComputePressureManager`, which broadcats the
information to the `ComputePressureHost` instances that it owns.

`content::ComputePressureQuantizer` implements the quantization logic that
converts a high-entropy `ComputePressureSample` into a low-entropy
`blink::mojom::ComputePressureState`, which minimizes the amount of information
exposed to a Web page that uses the Compute Pressure API. Each
`ComputePressureHost` uses a `ComputePressureQuantizer` instance, which stores
the origin's quantization schema and produces quantized data suitable for Web
pages served from that origin.
