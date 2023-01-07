# Compute Pressure API

This directory contains the browser-side implementation of the
[Compute Pressure API](https://github.com/wicg/compute-pressure/).

## Code map

The system is made up of the following components.

`blink::mojom::PressureService`, defined in Blink, is the interface between
the renderer and the browser sides of the API implementation.

`device::mojom::PressureManager`, defined in Services, is the interface
between the browser and the services sides of the API implementation.

`device::PressureManagerImpl` is the top-level class for the services side
implementation. The class is responsible for handling the communication
between the browser process and services.

`device::mojom::PressureUpdate` represents the device's compute pressure update,
composed of the `device::mojom::PressureState` and the timestamp.
This information is collected by `device::CpuProbe` and bubbled up by
`device::PlatformCollector` to `device::PressureManagerImpl`, which broadcasts
the information to the `content::PressureServiceImpl` instances.

`device::PlatformCollector` drives measuring the device's compute pressure
state. The class is responsible for invoking platform-specific measurement
code at regular intervals, and for straddling between sequences to meet
the platform-specific code's requirements.

`device::CpuProbe` is an abstract base class that interfaces between
`device::PlatformCollector` and platform-specific code that retrieves the
compute pressure state from the operating system. This interface is also
a dependency injection point for tests.

`content::PressureServiceImpl` serves the mojo connection for a frame.
Each instance is owned by a `content::RenderFrameHostImpl`. The class receives
`device::mojom::PressureUpdate` from `device::PressureManagerImpl` and
broadcasts the information to the `blink::PressureObserverManager` instance.

`blink::PressureObserver` implements bindings for the PressureObserver
interface. There can be more than one PressureObserver per frame.

`blink::PressureObserverManager` maintains the list of active observers.
The class receives `device::mojom::PressureUpdate` from
`content::PressureServiceImpl` and broadcasts the information to active
observers.
