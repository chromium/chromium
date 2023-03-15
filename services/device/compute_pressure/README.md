# Compute Pressure API

This directory contains the service-side implementation of the
[Compute Pressure API](https://github.com/w3c/compute-pressure/).

## Code map

The system is made up of the following components.

`device::mojom::PressureManager`, defined in Services, is the interface
between the renderer and the services sides of the API implementation.

`device::PressureManagerImpl` is the top-level class for the services side
implementation. The class is responsible for handling the communication
between the renderer process and services.

`device::mojom::PressureClient` is the interface that client of the
`device::mojom::PressureManager` interface must implement to receive
`device::mojom::PressureUpdate`.

`device::mojom::PressureUpdate` represents the device's compute pressure update,
composed of the `device::mojom::PressureState` and the timestamp.
This information is collected by `device::CpuProbe` and bubbled up by
`device::PlatformCollector` to `device::PressureManagerImpl`, which broadcasts
the information to the `blink::PressureObserverManager` instances.

`device::CpuProbe` is an abstract base class that drives measuring the
device's compute pressure state from the operating system. The class
is responsible for invoking platform-specific measurement code at
regular intervals, and for straddling between sequences to meet
the platform-specific code's requirements. This interface is also
a dependency injection point for tests.

`blink::PressureObserver` implements bindings for the PressureObserver
interface. There can be more than one PressureObserver per frame.

`blink::PressureObserverManager` maintains the list of active observers.
The class receives `device::mojom::PressureUpdate` from
`device::PressureManagerImpl` and broadcasts the information to active
observers.
