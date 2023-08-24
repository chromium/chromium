# Sensors

[TOC]

## Introduction

This document explains how sensor APIs (such as Ambient Light Sensor,
Accelerometer, Gyroscope, Magnetometer) are implemented in Chromium.

This directory contains the platform-specific parts of the
implementation, which is used, among others, by the [Generic Sensor
API](https://w3c.github.io/sensors/) and the [Device Orientation
API](https://w3c.github.io/deviceorientation/).

The document describes the Generic Sensor API implementation in both
the renderer and the browser process and lists important
implementation details, such as how data from a single platform sensor
is distributed among multiple JavaScript sensor instances and how
sensor configurations are managed.

## Background

The Generic Sensor API defines base interfaces that should be implemented by
concrete sensors. In most cases, concrete sensors should only define
sensor-specific data structures and, if required, sensor configuration options.

The same approach is applied to the implementation in Chromium, which was
designed with the following requirements in mind:

1.  Share the crucial parts of functionality between the concrete sensor
    implementations. Avoid the code duplication and thus simplify maintenance and
    development of new features.
1.  Support simultaneous existence and functioning of multiple JS Sensor
    instances of the same type that can have different configurations and
    lifetimes.
1.  Support for both “slow” sensors that provide periodic updates (e.g.
    Ambient Light, Proximity), and “fast” streaming sensors that have low-latency
    requirements for sensor reading updates (motion sensors).

**Note**: the implementation is architected in such a way that Blink (i.e.
`third_party/blink/renderer/modules/sensor`) is just a consumer of the data from
`services/device/generic_sensor` like any other. For example, the Blink [Device
Orientation API](/third_party/blink/renderer/modules/device_orientation/)
consumes sensor data from `//services` independently from the Blink Generic
Sensor implementation. The same applies to
[`device/vr/orientation`](/device/vr/orientation).

## Implementation Design

### Main components and APIs

The Generic Sensor API implementation consists of two main components: the
`sensor` module in Blink
([//third_party/blink/renderer/modules/sensor/](/third_party/blink/renderer/modules/sensor/))
which contains JS bindings for Generic Sensor API and concrete sensors APIs, and
the `generic_sensor` device service
([//services/device/generic_sensor/](/services/device/generic_sensor/)) \- a set
of classes running on the service process side that eventually call system APIs
to access the actual device sensor data.

The `//services` side also includes a few other directories:

*   `//services/device/public/cpp/generic_sensor` contains C++ classes and data
    structures used by both `//services/device/generic_sensor` as well as its
    consumers.
*   `//services/device/public/mojom` contains Mojo interfaces by the Generic
    Sensor implementation.
    *   [SensorProvider](/services/device/public/mojom/sensor_provider.mojom) is
        a “factory-like” interface that provides data about the sensors present
        on the device and their capabilities (reporting mode, maximum sampling
        frequency), and allows users to request a specific sensor.
    *   [Sensor](/services/device/public/mojom/sensor.mojom) is an interface
        wrapping a concrete device sensor.
    *   [SensorClient](/services/device/public/mojom/sensor.mojom) is
        implemented by Blink (and other consumers) to be notified about errors
        occurred on platform side and about sensor reading updates for sensors
        with ‘onchange’ reporting mode.

Actual sensor data is not passed to consumers (such as Blink) via Mojo
calls \- a shared memory buffer is used instead, thus we avoid filling up the
Mojo IPC channel with sensor data (for sensors with continuous reporting mode) when
the platform sensor has a high sampling frequency, and also avoid adding extra
latency.

A high-level diagram of the Mojo architecture looks like this:

![Generic Sensor Framework component diagram](docs/generic_sensor_framework_component_diagram.png)

#### Sensor Fusion

Some sensors provide data that is obtained by combining readings from other
sensors (so-called low-level sensors). This process is called **sensor fusion**.
It can be done in hardware or software.

In Chromium, we sometimes perform software sensor fusion when a certain
hardware sensor is not available but "fusing" readings from other sensors
provides a similar reading. The fusion process involves reading data from one or
more sensors and applying a fusion algorithm to derive another reading from them
(possibly in a different unit).

The figure below figure shows an overview of the fusion sensor flow:

![Overview of fusion sensor flow](docs/overall_picture_of_sensor_flow.png)

In the code, the main classes are
[`PlatformSensorFusion`](platform_sensor_fusion.h) and
[`PlatformSensorFusionAlgorithm`](platform_sensor_fusion_algorithm.h).

`PlatformSensorFusion` owns a `PlatformSensorFusionAlgorithm` instance. It
inherits from both `PlatformSensor` as well as `PlatformSensor::Client`. The
former indicates it can be treated by consumers as a regular sensor, while the
latter means that it subscribes to updates from low-level sensors (like
[`SensorImpl`](sensor_impl.h) itself).  It is in its implementation of
`OnSensorReadingChanged()` that it invokes its `PlatformSensorFusionAlgorithm`
to fuse data from the underlying sensors.

Once any of the low-level sensors receive a new value, it notifies its clients
(including the fusion sensor). The fusion sensor algorithm reads the low-level
sensor raw values and outputs a new reading, which is fed to
`PlatformSensor::UpdateSharedBufferAndNotifyClients()` as usual.

### Security and Privacy

Platform sensor readings can expose more information about a device and
consequently lead to an increase in the [fingerprinting
surface](https://w3c.github.io/fingerprinting-guidance/) exposed by the
browser, eavesdropping, and keystroke monitoring.

The security and anti-fingerprinting considerations are also based on existing
literature on the topic, especially research on sensors exposed to native
applications on mobile devices:

*   [Gyrophone: Recognizing Speech from Gyroscope
    Signals](https://crypto.stanford.edu/gyrophone/files/gyromic.pdf)
*   [ACCessory: password inference using accelerometers on
    smartphones](https://pdfs.semanticscholar.org/3673/2ae9fbf61f84eab43e60bc2bcb0a48d05b67.pdf)
*   [Touchsignatures: identification of user touch actions and pins based on
    mobile sensor data via javascript](https://arxiv.org/pdf/1602.04115.pdf)
*   [SoK: Systematic Classification of Side-Channel Attacks on Mobile
    Devices](https://arxiv.org/pdf/1611.03748.pdf)
*   [Pin skimming: Exploiting the ambient-light sensor in mobile
    devices](https://arxiv.org/pdf/1405.3760.pdf)
*   [TapLogger: Inferring User Inputs On Smartphone Touchscreens Using On-board
    Motion Sensors](https://pdfs.semanticscholar.org/c860/4311321f1b8f8fdc8acff8871a5bad2ad4ac.pdf)

The Generic Sensor implementation in Chromium follows the [Mitigation
Strategies](https://w3c.github.io/sensors/#mitigation-strategies) section of
the Generic Sensor API specification. Namely, this means that:

*   The sensor APIs are only exposed to secure contexts (the same also applies
    to the API exposed by the [Device Orientation
    spec](https://w3c.github.io/deviceorientation/#idl-index)).
*   There is integration with both the Permissions API and the Permissions
    Policy API.
*   Sensor readings are only available to documents whose visibility state is
    "visible" (this also applies to the Device Orientation API).
*   Sensor readings are only available for active documents whose origin is same
    origin-domain with the currently focused area document.

The Chromium implementation also applies additional privacy measures (some of
which are making their way back to the specification):

*   **Frequency**: The maximum sampling frequency is
    [capped](/services/device/public/cpp/generic_sensor/sensor_traits.h) at 60Hz
    for most sensor types. Ambient Light sensors and magnetometers are capped at
    10Hz.
*   **Accuracy**: Readings are [quantized](#Rounding), and for some sensor types
    readings which do not differ by a certain [threshold](#Threshold-checks) are
    discarded and never exposed.

There is no distinction in how the Generic Sensor APIs are exposed to regular
and incognito windows.

### Classes & APIs

#### //services main classes

*   `PlatformSensorProvider`: singleton class whose main functionality is to
    create and track `PlatformSensor` instances. `PlatformSensorProvider` is
    also responsible for creating a shared buffer for sensor readings. Every
    platform has its own implementation of `PlatformSensorProvider`
    (`PlatformSensorProviderAndroid`, `PlatformSensorProviderWin` etc).

*   `PlatformSensor`: represents device sensor of a given type. There can be
    only one `PlatformSensor` instance of the same type at a time, its ownership
    is shared between existing `SensorImpl` instances. `PlatformSensor` is an
    abstract class which encapsulates generic functionality and is inherited by
    the platform-specific implementations (`PlatformSensorAndroid`,
    `PlatformSensorWin` etc).

*   `SensorImpl`: implements the exposed `Sensor` Mojo interface and forwards
    IPC calls to the owned PlatformSensor instance. `SensorImpl` implements the
    `PlatformSensor::Client` interface to receive notifications from
    `PlatformSensor`.

*   `SensorProviderImpl`: implements the exposed `SensorProvider` Mojo interface
    and forwards IPC calls to the `PlatformSensorProvider` singleton instance.

The classes above have the following ownership relationships:

*   `SensorProviderImpl` owns a single `PlatformSensorProvider` instance via a
    `std::unique_ptr`.
*   `SensorProviderImpl` owns all `SensorImpl` instances via a
    `mojo::UniqueReceiverSet`.
*   `PlatformSensor` is a ref-counted class, and a `SensorImpl` has a reference
    to a `PlatformSensor`.
*   `DeviceService` owns a single `SensorProviderImpl` instance.
    `DeviceService::BindSensorProvider()` is responsible for creating a
    `PlatformSensorProvider` if one does not exist and pass it to
    `SensorProviderImpl`.

#### Blink main classes

*   `Sensor`: implements bindings for the `Sensor` IDL interface. All classes
    that implement concrete sensor interfaces (such as `AmbientLightSensor`,
    `Gyroscope`, `Accelerometer`) must inherit from it.

*   `SensorProviderProxy`: owns one side of the `SensorProvider` Mojo interface
    pipe and manages `SensorProxy` instances. This class supplements
    `DOMWindow`, so `Sensor` obtains a `SensorProviderProxy` instance via
    `SensorProviderProxy::From()` and uses it to the get `SensorProxy` instance
    for a given sensor type.

*   `SensorProxy`: owns one side of the `Sensor` Mojo interface and implements
    the `device::mojom::blink::SensorClient` Mojo interface. It also defines a
    `SensorProxy::Observer` interface that is used to notify `Sensor` and its
    subclasses of errors or data updates from the platform side. `Sensor` and
    its subclasses interact with the `//services` side via `SensorProxy` (and
    `SensorProviderProxy`) rather than owning the Mojo pipes themselves.

In a `LocalDOMWindow`, there is one `SensorProxy` instance for a given sensor
type (ambient light, accelerometer, etc) whose ownership is shared among
`Sensor` instances. `SensorProxy` instances are created when `Sensor::start()`
is called and are destroyed when there are no more active Sensor instances left.

### Code flow

#### Low-level sensor

The figure below shows the code flow for a low-level (i.e. non-fusion) sensor:

![Low-level sensor
flow](docs/low_level_sensor_flow.png)

Each OS-specific `PlatformSensor` implementation retrieves sensor readings
differently, but they all ultimately call
`PlatformSensor::UpdateSharedBufferAndNotifyClients()`. This function invokes
`PlatformSensor::UpdateSharedBuffer()` in
[`platform_sensor.cc`](platform_sensor.cc), which checks and transforms a
reading before storing it in the shared buffer:

1.  Sensors whose reporting mode is `mojom::ReportingMode::ON_CHANGE` (i.e. they
    only send notifications when the reading has changed) first check if the new
    value is different enough compared to the current value. What is considered
    different enough (i.e. the threshold check) varies per sensor type (see
    `PlatformSensor::IsSignificantlyDifferent()` for the base implementation).
    [Threshold](#threshold)-chapter has more information why code uses threshold
    value. And [Used threshold values](#used-threshold-values)-chapter has the
    actual values.
1.  If the check above passes, the so-called "raw" (unrounded) reading is stored
    to [`last_raw_reading_`](platform_sensor.h).
1.  The reading is rounded via `RoundSensorReading()` (in
    [`platform_sensor_util.cc`](platform_sensor_util.cc)) using a per-type
    algorithm. [Rounding](#rounding)-chapter has more information why sensor
    values are rounded.
1.  The rounded reading is stored in the shared buffer and becomes the value
    that clients can read.

#### Fusion sensor

Fusion sensors behave similarly, but with extra steps at the end:

1.  They get notified of a new reading when
    `PlatformSensor::UpdateSharedBufferAndNotifyClients()` invokes
    `PlatformSensorFusion::OnSensorReadingChanged()`.
1.  `PlatformSensorFusion::OnSensorReadingChanged()` invokes the sensor's fusion
    algorithm, which fetches the low-level sensors' **raw** readings and fuses
    them.
1.  It invokes `UpdateSharedBufferAndNotifyClients()`, which will go through the
    same threshold check and rounding process described above, but for the fused
    reading.

The figure below shows an example of the code flow:

![Fusion sensor
flow](docs/fusion_sensor_flow.png)

#### Rounding and threshold checks

##### Rounding

Rounding is a form of
[quantization](https://en.wikipedia.org/wiki/Quantization_(signal_processing)).
It is used to reduce the accuracy of raw sensor readings in order to help reduce
the fingerprinting surface exposed by sensors. For example, instead of exposing
an accelerometer reading of 12.34567m/s^2, we expose a value of 12.3m/s^2
instead. https://crbug.com/1018180 and https://crbug.com/1031190 show examples
of issues we try to avoid by providing rounded readings.

Choosing how much rounding to apply is a balance between reducing the
fingerprinting surface while also still providing values that are meaningful
enough to users. [`platform_sensor_util.h`](platform_sensor_util.h) contains the
values we use for each different sensor type.

Currently magnetometer, pressure and proximity sensor readings are not rounded
(magnetometer is not exposed by default, and there is no backend implementation
for pressure and proximity sensors).

##### Threshold checks

To prevent small changes around the rounding border from triggering
notifications, a threshold check is performed and readings that fail it are
discarded. Rounding is the main measure to prevent fingerprinting and data
leaks, and the threshold checks play an auxiliary role in conjunction with it by
reducing the number of updates when a raw reading is hovering around a certain
value.

**Note:** see the discussions in https://crrev.com/c/3666917 and
https://crbug.com/1332536 about the role threshold checks play as a mitigation
strategy. On the one hand, there is no mathematical analysis behind its use as a
security measure, on the other hand we know that it does reduce the number of
updates and does not increase the attack surface.

**Note:** threshold checks are only performed for sensors whose reporting mode
is `ON_CHANGE`. We consider that for sensors with a `CONTINUOUS` reporting mode
it is more important to report readings at a certain rate than to ignore similar
readings.

For example, if the code rounded readings to the nearest multiple of 50 and no
threshold checks were done, a change from a raw value of 24 (rounded to 0) to
25 (rounded to 50) would trigger a new reading notification, whereas requiring
the raw readings to differ by 25 guarantees that the readings must differ more
significantly to trigger a new notification.

The actual checks differ by sensor type:

*   Ambient Light Sensor readings must differ by half of the rounding multiple
    (`kAlsRoundingMultiple`), or 25 lux in practice.
*   Fusion sensors default to requiring a difference of 0.1 in readings, which
    can be changed by calling `PlatformSensorFusionAlgorithm::set_threshold()`
*   Other low-level sensors only check for equality between current and new
    readings at the moment.

##### Execution order of rounding and threshold check

We first perform a threshold check on the current and new raw readings and only
perform rounding and store the new rounded reading if the threshold check
passes.

If we did rounding first, we would increase even more the area which rounding
already protects, and callers would get values that are too inaccurate.

#### Permissions

##### Permissions API integration

The [`Sensor.start()`](https://w3c.github.io/sensors/#sensor-start) operation
in the Generic Sensor spec invokes the [request sensor
access](https://w3c.github.io/sensors/#request-sensor-access) abstract
operation so that permission checks (via the [Permissions
API](https://w3c.github.io/permissions/) are performed before a sensor is
started.

In Chromium, the permission checks are done in the `//content/browser` side:
`SensorProviderProxyImpl::GetSensor()` invokes
`PermissionController::RequestPermissionFromCurrentDocument()` and only
connects to the `//services` side if permission has been granted.

**Note**: At the moment, users are never prompted to grant access to a device's
sensors, access is either allowed or denied. This ended up happening for
historical reasons: the Device Orientation API was implemented first and neither
spec nor implementation were supposed to prompt for sensor access (it was
retrofitted into the spec years later), the Generic Sensor API was added to
Chromium later, the Device Orientation API in Blink was changed to use the same
backend in `//services`, and the permission behavior was kept to avoid breaking
user compatibility. Work to improve the situation for both the Device
Orientation API and the Generic Sensor API in Chromium is tracked in
https://crbug.com/947112.

From a UI perspective, users are able to grant or deny access to sensors in the
global settings (chrome://settings/content) or on a per-site basis even before
work on issue 947112 is finished.

##### Permissions policy integration

The Chromium implementation follows the spec and integrates with the Permissions
Policy spec.

The [Policy Controlled
Features](https://github.com/w3c/webappsec-permissions-policy/blob/main/features.md)
page lists the current status of different features.

Blink performs checks in
[`Sensor::Sensor()`](/third_party/blink/renderer/modules/sensor/sensor.cc) by
calling `AreFeaturesEnabled()`, while the `content/` side also performs checks
in
[`SensorProviderProxyImpl::CheckFeaturePolicies()`](/content/browser/generic_sensor/sensor_provider_proxy_impl.cc).

#### Reporting values to consumer

##### Reporting modes

Sensors have two ways of reporting new values as defined in
[`ReportingMode`](/services/device/public/mojom/sensor.mojom). The values are
based on the [Android sensor reporting
modes](https://source.android.com/devices/sensors/report-modes):

*   `ON_CHANGE`: Clients are notified via
    `PlatformSensor::Client::OnSensorReadingChanged()` when the measured values
    have changed. Different sensor types have different thresholds for
    considering whether a change is significant enough to be reported as
    discussed in [the round and thresholds
    section](#rounding-and-threshold-checks).
*   `CONTINUOUS`: Sensor readings are continuously updated at a frequency
    derived from
    [`PlatformSensorConfiguration.frequency`](/services/device/public/cpp/generic_sensor/platform_sensor_configuration.h).
    The sampling frequency value is capped to 10Hz or 60Hz (defined in
    [`sensor_traits.h`](/services/device/public/cpp/generic_sensor/sensor_traits.h))
    for security reasons as explained in the [Security and
    Privacy](#Security-and-Privacy) section of this document.
    `PlatformSensor::Client::OnSensorReadingChanged()` is never invoked, and
    clients are expected to periodically fetch readings on their own via
    `SensorReadingSharedBufferReader::GetReading()`.

There is no default reporting mode: `PlatformSensor` subclasses are expected to
implement the `GetReportingMode()` method and return a value fit for the given
OS and sensor type.

##### Configuration and frequency management

As described above, there is always at most one `PlatformSensor` instance for a
given sensor type, and it can have multiple clients consuming its readings. Each
client might be interested in receiving readings at a different frequency, and
it is up to `PlatformSensor` to coordinate these requests and choose a reading
frequency.

Clients first invoke the [`SensorProvider.GetSensor()` Mojo
method](/services/device/public/mojom/sensor_provider.mojom); on success, the
return values include a [`Sensor` Mojo
interface](/services/device/public/mojom/sensor.mojom). Clients then invoke the
`Sensor.AddConfiguration()` and `Sensor.RemoveConfiguration()` to add and remove
`SensorConfiguration` instances from the given `Sensor`.

When a new configuration is added, `PlatformSensor::UpdateSensorInternal()` is
called and will ultimately choose the highest of all requested frequencies
(within the boundaries defined in
[sensor_traits.h](/services/device/public/cpp/generic_sensor/sensor_traits.h)
for security reasons). In other words, a client's requested frequency does not
necessarily match `PlatformSensor`'s actual sampling frequency (or the frequency
the OS ends up using).

It is then up to each client to check a reading's timestamp and decide whether
to ignore it if not enough time has passed since the previous reading was
delivered. It is also possible for clients to simply process all readings
regardless of the actual sampling frequency.

Blink adopts the former strategy:

*   For sensors with a `CONTINUOUS` reporting mode, `SensorProxyImpl` polls the
    shared memory buffer at the highest requested frequency. For sensors with a
    `ON_CHANGE` reporting mode, it will receive updates from the services side
    via `SensorProxyImpl::SensorReadingChanged()`.
*   Each individual `Sensor` instance connected to a `SensorProxyImpl` will be
    notified via `Sensor::OnSensorReadingChanged()`, which takes care of
    avoiding sending too many "reading" JS notifications depending on the Sensor
    instance's requested frequency.

##### Sensor readings shared buffer

As mentioned above, sensor readings are not transmitted from the services side
to the consumers (such as the renderer process) via IPC calls, but with a
[shared memory buffer](/base/memory/read_only_shared_memory_region.h).
Read-write operations are synchronized via a seqlock mechanism.

The `PlatformSensorProvider` singleton owned by `DeviceService` maintains a
shared memory mapping of the entire buffer. Only `PlatformSensorProvider` has
write access to it; all consumers only have read-access to the buffer. This
shared buffer is a contiguous sequence of `SensorReadingSharedBuffer`s, one per
`mojom::SensorType`.

Each `SensorReadingBuffer` structure has 6 tightly-packed 64-bit floating
fields: **seqlock**, **timestamp**, **sensor reading 1**, **sensor reading 2**,
**sensor reading 3**, and **sensor reading 4**. This has a fixed size of 6 * 8 =
48 bytes. The whole shared buffer's size depends on `mojom::SensorType`'s size;
at the time of writing, there are 12 distinct sensor types, which means the
entire shared buffer handle is 12 * 48 = 576 bytes in size.
[sensor_reading.h](/services/device/public/cpp/generic_sensor/sensor_reading.h)
has the actual `SensorReading` data structure as well as the
`SensorReadingSharedBuffer` structure.

The code treats each `SensorReadingBuffer` embedded in the mapping as completely
independent from the others: `SensorReadingSharedBuffer::GetOffset()` provides
the offset in the buffer corresponding to the `SensorReadingBuffer` representing
a given sensor type, `SensorReadingSharedBufferReader::Create()` requires an
offset to map only a specific portion of the shared buffer and, additionally,
each `SensorReadingBuffer` has its own `seqlock` for coordinating reads and
writes.

![Shared buffer diagram](docs/shared_buffer.png)

In terms of code flow, the creation and sharing of the shared buffer when
between Blink calls `SensorProviderProxy::GetSensor()` works roughly like this:

1.  `SensorProviderProxy::GetSensor()` calls the `GetSensor()` operation in the
    Sensor Mojo interface.
1.  That is implemented by `SensorProviderImpl::GetSensor()`. It invokes
    `PlatformSensorProvider::CloneSharedMemoryRegion()`.
    1.  `PlatformSensorProvider::CloneSharedMemoryRegion()` initializes the
        shared buffer and the mapping if necessary in
        `PlatformSensorProvider::CreateSharedBufferIfNeeded()`.
    1.  `PlatformSensorProvider::CloneSharedMemoryRegion()` returns a
        read-only mapping handle.
1.  If the platform sensor still needs to be created,
    `SensorProviderImpl::GetSensor()` will invoke
    `PlatformSensorProvider::CreateSensor()`, which invokes
    `PlatformSensorProvider::GetSensorReadingSharedBufferForType()`. The
    `SensorReadingSharedBuffer` pointer returned by this function is later
    passed to `PlatformSensor` and its subclasses, which update it when the
    readings change.
1.  Ultimately, `PlatformSensorProvider::SensorCreated()` is called, and the
    read-only shared buffer handle and the offset in it corresponding to the
    sensor type being created are passed in `mojom::SensorInitParams`.
1.  On the Blink side, `SensorProxyImpl::OnSensorCreated()` is invoked with the
    `SensorInitParams` initialized above. It invokes
    `SensorReadingSharedBufferReader::Create()` with the buffer and offset from
    `SensorInitParams`, and that is what it later used to obtain readings.

### Platform-specific details

#### Android

The Android implementation in `//services` consists of two parts, native (C++)
and Java. The native side includes `PlatformSensorProviderAndroid` and
`PlatformSensorAndroid`, while the Java side consists of the
`PlatformSensorProvider` and `PlatformSensor` classes that are included in the
`org.chromium.device.sensors` package. Java classes interface with Android
Sensor API to fetch readings from device sensors. The native and Java sides
communicate via JNI.

The [`PlatformSensorProviderAndroid`](platform_sensor_provider_android.h) C++
class inherits from `PlatformSensorProvider` and is responsible for creating a
`PlatformSensorProvider` (Java) instance via JNI. When the Java object is
created, all sensor creation requests are forwarded to the Java object.

The [`PlatformSensorAndroid`](platform_sensor_android.h) C++ class inherits from
`PlatformSensor`, owns the `PlatformSensor` Java object and forwards start, stop
and other requests to it.

The
[`PlatformSensorProvider`](android/java/src/org/chromium/device/sensors/PlatformSensorProvider.java)
Java class is responsible for thread management, and `PlatformSensor` creation.
It also owns the Android `SensorManager` object that is accessed by
`PlatformSensor` Java objects.

The
[`PlatformSensor`](android/java/src/org/chromium/device/sensors/PlatformSensor.java)
Java class implements the `SensorEventListener` interface and owns the Android
`Sensor` object. `PlatformSensor` adds itself as an event listener to receive
sensor reading updates and forwards them to native side using the `native*`
methods.

#### Windows

The Windows backend has its [own README.md](windows/README.md).

#### ChromeOS

On ChromeOS, sensors are implemented with Mojo connections to IIO Service, a
CrOS daemon that provides sensors' data to other applications.

_Need to add information about iio service and the platform-specific
implementation that reads sensor data and feeds it to Chromium_

#### Linux

Sensor data is exposed by the Linux kernel via its Industrial I/O (iio)
subsystem. Sensor readings and metadata are readable via sysfs; udev is used for
device enumeration and getting notifications about sensors that are added or
removed from the system.

[`PlatformSensorProviderLinuxBase`](platform_sensor_provider_linux_base.h) is
shared between the Linux and ChromeOS implementations, and
[`PlatformSensorProviderLinux`](platform_sensor_provider_linux.h) is its
Linux-specific subclass. Similarly,
[`PlatformSensorLinux`](platform_sensor_linux.h) inherits from the base
`PlatformSensor` class.

The implementation is supported by a few more classes:

*   [`SensorDeviceManager`](linux/sensor_device_manager.h): interfaces with udev
    by subclassing `device::UdevWatcher::Observer`. It enumerates all sensors
    exposed by the Linux kernel via udev, caches their information (frequency,
    scaling value, sysfs location etc) and notifies users (i.e.
    `PlatformSensorProviderLinux`) of changes later.
*   [`SensorInfoLinux`](linux/sensor_data_linux.h): gathers all iio sensor
    information in a single struct (frequency, scaling value, sysfs location
    etc).
*   [`SensorReader`](platform_sensor_reader_linux.h) and
    [`PollingSensorReader`](platform_sensor_reader_linux.cc) implement the
    reading and reporting of sensor data from sysfs.

When a request for a specific type of sensor comes and
`PlatformSensorProviderLinux::CreateSensorInternal()` is called, it will either
cause `SensorDeviceManager` to start enumerating all available sensors if that
has not happened yet or look for a specific sensor in `SensorDeviceManager`'s
cache. That is then used to construct a new `PlatformSensorLinux` object (which
takes a `SensorInfoLinux` pointer), which then owns a `SensorReader` that uses
the `SensorInfoLinux` to know which sensor data to read and how to parse it.

#### macOS

`PlatformSensorProviderMac` implements `PlatformSensorProvider` and is
responsible for creating one of the two classes implementing sensor support on
Mac:

*   [`PlatformSensorAccelerometerMac`](platform_sensor_accelerometer_mac.h)
    exposes accelerometer data and can be in sensor fusion (e.g.
    [`RelativeOrientationEulerAnglesFusionAlgorithmUsingAccelerometer`](relative_orientation_euler_angles_fusion_algorithm_using_accelerometer.h)).
    It is backed by the
    [`SuddenMotionSensor`](/third_party/sudden_motion_sensor/sudden_motion_sensor_mac.h)
    code in third_party and thus relies on being able to read from the
    SMCMotionSensor.
*   [`PlatformSensorAmbientLightMac`](platform_sensor_ambient_light_mac.h)
    exposed ambient light data from laptop models that support and expose it.

As evidenced above, macOS provides limited support for reading sensor data, and
it is mostly restricted to older laptop models. In the future, it might be
possible to remove all the Mac code from `//services/device/generic_sensor`.
