# Sensors

`services/device/generic_sensor` contains the platform-specific parts of the Sensor APIs
implementation.

Sensors Mojo interfaces are defined in the `services/device/public/mojom` subdirectory.

## Web-exposed Interfaces

### [Generic Sensors](https://www.w3.org/TR/generic-sensor/)

The Generic Sensors API is implemented in `third_party/blink/renderer/modules/sensor` and exposes the following sensor types as JavaScript objects:

* [AbsoluteOrientationSensor] &rarr; ABSOLUTE_ORIENTATION_QUATERNION
* [Accelerometer] &rarr; ACCELEROMETER
* [AmbientLightSensor] &rarr; AMBIENT_LIGHT
* [Gyroscope] &rarr; GYROSCOPE
* [LinearAccelerationSensor] &rarr; LINEAR_ACCELEROMETER
* [Magnetometer] &rarr; MAGNETOMETER
* [RelativeOrientationSensor] &rarr; RELATIVE_ORIENTATION_QUATERNION

[AbsoluteOrientationSensor]: ../../../third_party/blink/renderer/modules/sensor/absolute_orientation_sensor.idl
[Accelerometer]: ../../../third_party/blink/renderer/modules/sensor/accelerometer.idl
[AmbientLightSensor]: ../../../third_party/blink/renderer/modules/sensor/ambient_light_sensor.idl
[Gyroscope]: ../../../third_party/blink/renderer/modules/sensor/gyroscope.idl
[LinearAccelerationSensor]: ../../../third_party/blink/renderer/modules/sensor/linear_acceleration_sensor.idl
[Magnetometer]: ../../../third_party/blink/renderer/modules/sensor/magnetometer.idl
[RelativeOrientationSensor]: ../../../third_party/blink/renderer/modules/sensor/relative_orientation_sensor.idl

### [DeviceOrientation Events](https://www.w3.org/TR/orientation-event/)

The DeviceOrientation Events API is implemented in `third_party/blink/renderer/modules/device_orientation` and exposes two events based on the following sensors:

* [DeviceMotionEvent]
  * ACCELEROMETER: populates the `accelerationIncludingGravity` field
  * LINEAR_ACCELEROMETER: populates the `acceleration` field
  * GYROSCOPE: populates the `rotationRate` field
* [DeviceOrientationEvent]
  * ABSOLUTE_ORIENTATION_EULER_ANGLES (when a listener for the `'deviceorientationabsolute'` event is added)
  * RELATIVE_ORIENTATION_EULER_ANGLES (when a listener for the `'deviceorientation'` event is added)

[DeviceMotionEvent]: ../../../third_party/blink/renderer/modules/device_orientation/device_motion_event.idl
[DeviceOrientationEvent]: ../../../third_party/blink/renderer/modules/device_orientation/device_orientation_event.idl

The content renderer layer is located in `third_party/blink/renderer/modules/device_orientation`.

Testing:

* Browser tests are located in `content/browser/device_sensors`.
* Layout tests are located in `third_party/WebKit/LayoutTests/device_orientation`.
* Web platform tests are located in `third_party/WebKit/LayoutTests/external/wpt/orientation-event` and are a mirror of the [web-platform-tests GitHub repository](https://github.com/web-platform-tests/wpt).

## Permissions

The device service provides no support for permission checks. When the render process requests access to a sensor type this request is proxied through the browser process by [SensorProviderProxyImpl] which is responsible for checking the permissions granted to the requesting origin.

[SensorProviderProxyImpl]: ../../../content/browser/generic_sensor/sensor_provider_proxy_impl.h

## Platform Support

Support for the SensorTypes defined by the Mojo interface is summarized in this
table. An empty cell indicates that the sensor type is not supported on that
platform.

| SensorType                        | Android                   | Linux                                 | macOS                                 | Windows                                   |
| --------------------------------- | ------------------------- | ------------------------------------- | ------------------------------------- | ----------------------------------------- |
| AMBIENT_LIGHT                     | TYPE_LIGHT                | in_illuminance                        | AppleLMUController                    | Yes                                       |
| PROXIMITY                         |                           |                                       |                                       |                                           |
| ACCELEROMETER                     | TYPE_ACCELEROMETER        | in_accel                              | SMCMotionSensor                       | Yes                                       |
| LINEAR_ACCELEROMETER              | See below                 | ACCELEROMETER (*)                     |                                       | ACCELEROMETER (*)                         |
| GYROSCOPE                         | TYPE_GYROSCOPE            | in_anglvel                            |                                       | Yes                                       |
| MAGNETOMETER                      | TYPE_MAGNETIC_FIELD       | in_magn                               |                                       | Yes                                       |
| PRESSURE                          |                           |                                       |                                       |                                           |
| ABSOLUTE_ORIENTATION_EULER_ANGLES | See below                 | ACCELEROMETER and MAGNETOMETER (*)    |                                       | Yes                                       |
| ABSOLUTE_ORIENTATION_QUATERNION   | See below                 | ABSOLUTE_ORIENTATION_EULER_ANGLES (*) |                                       | Yes                                       |
| RELATIVE_ORIENTATION_EULER_ANGLES | See below                 | ACCELEROMETER and GYROSCOPE (*)       | ACCELEROMETER (*)                     |                                           |
|                                   |                           | or ACCELEROMETER (*)                  |                                       |                                           |
| RELATIVE_ORIENTATION_QUATERNION   | TYPE_GAME_ROTATION_VECTOR | RELATIVE_ORIENTATION_EULER_ANGLES (*) | RELATIVE_ORIENTATION_EULER_ANGLES (*) |                                           |

(Note: "*" means the sensor type is provided by sensor fusion.)

### Android

Sensors are implemented by passing through values provided by the
[Sensor](https://developer.android.com/reference/android/hardware/Sensor.html)
class. The TYPE_* values in the below descriptions correspond to the integer
constants from the android.hardware.Sensor used to provide data for a
SensorType.

For LINEAR_ACCELEROMETER, the following sensor fallback is used:
1. Use TYPE_LINEAR_ACCELERATION directly
2. ACCELEROMETER, with a low-pass filter to isolate the effect of gravity

For ABSOLUTE_ORIENTATION_EULER_ANGLES, the following sensor fallback is used:
1. ABSOLUTE_ORIENTATION_QUATERNION (if it uses TYPE_ROTATION_VECTOR
     directly)
2. Combination of ACCELEROMETER and MAGNETOMETER

For ABSOLUTE_ORIENTATION_QUATERNION, the following sensor fallback is used:
1. Use TYPE_ROTATION_VECTOR directly
2. ABSOLUTE_ORIENTATION_EULER_ANGLES

For RELATIVE_ORIENTATION_EULER_ANGLES, converts the data produced by
RELATIVE_ORIENTATION_QUATERNION to euler angles.

### Linux (and Chrome OS)

Sensors are implemented by reading values from the IIO subsystem. The values in
the "Linux" column of the table above are the prefix of the sysfs files Chrome
searches for to provide data for a SensorType. The
ABSOLUTE_ORIENTATION_EULER_ANGLES sensor type is provided by interpreting the
value that can be read from the ACCELEROMETER and MAGNETOMETER. The
ABSOLUTE_ORIENTATION_QUATERNION sensor type is provided by interpreting the
value that can be read from the ABSOLUTE_ORIENTATION_EULER_ANGLES. The
RELATIVE_ORIENTATION_EULER_ANGLES sensor type is provided by interpreting the
value that can be read from the ACCELEROMETER and GYROSCOPE, or ACCELEROMETER.
The RELATIVE_ORIENTATION_QUATERNION sensor type is provided by interpreting the
value that can be read from the RELATIVE_ORIENTATION_EULER_ANGLES.
LINEAR_ACCELEROMETER sensor type is provided by implementing a low-pass-filter
over the values returned by the ACCELEROMETER in order to remove the
contribution of the gravitational force.

### macOS

On this platform there is limited support for sensors. The AMBIENT_LIGHT sensor
type is provided by interpreting the value that can be read from the LMU. The
ACCELEROMETER sensor type is provided by interpreting the value that can be read
from the SMCMotionSensor. The RELATIVE_ORIENTATION_EULER_ANGLES sensor type is
provided by interpreting the value that can be read from the ACCELEROMETER. The
RELATIVE_ORIENTATION_QUATERNION sensor type is provided by interpreting the
value that can be read from the RELATIVE_ORIENTATION_EULER_ANGLES.

### Windows

Please refer to this [document](windows/README.md).

## Testing

Sensors platform unit tests are located in the current directory and its
subdirectories.

The sensors unit tests file for Android is
`android/junit/src/org/chromium/device/sensors/PlatformSensorAndProviderTest.java`.

Sensors browser tests are located in `content/test/data/generic_sensor`.

## Design Documents

Please refer to the [design documentation](https://docs.google.com/document/d/1Ml65ZdW5AgIsZTszk4mD_ohr40pcrdVFOIf0ZtWxDv0)
for more details.
