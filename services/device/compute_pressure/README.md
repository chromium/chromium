# Compute Pressure API

This directory contains the service-side implementation of the
[Compute Pressure API](https://github.com/w3c/compute-pressure/).

## Code map

The system is made up of the following components.

`device::mojom::PressureManager`, defined in services, is the interface
implemented in the browser side (`content::PressureServiceBase`) to communicate
with the renderer and implemented in services (`device::PressureManagerImpl`)
to communicate with the browser side.

`device::PressureManagerImpl` is the top-level class for the services side
implementation. The class is responsible for handling the communication
between the browser and services sides.

`device::mojom::PressureClient` is the interface that client of the
`device::mojom::PressureManager` interface must implement to receive
`device::mojom::PressureUpdate`.

`device::mojom::PressureUpdate` represents the device's compute pressure update,
composed of the `device::mojom::PressureState` and the timestamp.
This information is collected by `device::CpuProbe` and bubbled up by
`device::PlatformCollector` to `device::PressureManagerImpl`, which broadcasts
the information to the `content::PressureClientImpl` instances first and then
to `blink::PressureClientImpl` instances.

`device::CpuProbeManager` is an interface for measuring the device's compute
pressure state. It maintains a `system_cpu::CpuProbe` and requests CPU samples
from it at regular intervals.

`content::PressureService*` is the bridge between the renderer and the
services sides. This class maintains `content::PressureClientImpl` instances
per source type.

`content::PressureClientImpl` implements the `device::mojom::PressureClient`
interface to receive `device::mojom::PressureUpdate` from
`device::PressureManagerImpl` instance and broadcasts the information to the
`blink::PressureClientImpl` instance.

`blink::PressureObserver` implements bindings for the PressureObserver
interface. There can be more than one PressureObserver per frame.

`blink::PressureObserverManager` keeps track of `blink::PressureClientImpl` and
the connection to the `content::PressureService*` instance. The class is
responsible for handling the communication between the renderer and browser
sides.

`blink::PressureClientImpl` implements the `device::mojom::PressureClient`
interface to receive `device::mojom::PressureUpdate` from
`content::PressureClientImpl` and broadcasts the information to active
`blink::PressureObserver`. This class also keeps track of State and active
`blink::PressureObserver` per source type.
