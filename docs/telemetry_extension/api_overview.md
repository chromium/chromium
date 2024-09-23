# Telemetry Extension API overview

This document gives a function-level documentation of the Telemetry Extension
API. It is separated in the three namespaces **telemetry**, **diagnostics**,
**events** and **management**.

[TOC]

# Dictionary-based union types

There are functions with similar signatures. It's useful to simplify the
interfaces by merging these functions into one. For example,
`postFooData(arg: FooType)` and `postBarData(arg: BarType)` could be merged into
`postData(arg: ExampleUnionType)`. We use a *dictionary-based union type* for
this purpose. Such a union object represents exactly one of the `N` candidate
types, in the form of a dictionary with `N` optional fields, each corresponding
to one candidate type.

Let's see an example. The following table describes a union type
`ExampleUnionType` that is either a `FooType` or a `BarType`.

| Property Name | Type | Description |
------------ | ------- | ----------- |
| foo | FooType | Information about foo |
| bar | BarType | Information about bar |

To invoke a function `os.exampleFunction()` that accepts an `ExampleUnionType`
* with an object of `FooType`
    ```
    os.exampleFunction(
      arg: {
         foo: aFooObject
      }
    )
    ```
* with an object of `BarType`
    ```
    os.exampleFunction(
      arg: {
         bar: aBarObject
      }
    )
    ```

Notice that exactly one field should be set. That is, the object is invalid
when either
* more than one field is set, or
* no field is set.

# Diagnostics

The diagnostics namespace got a rework since the M119 release and added a new
extension-event based interface in M119. The interface is described in
[V2 Diagnostics API](#v2-diagnostics-api).

## Types

### Enum RoutineType
| Property Name |
------------ |
| ac_power |
| battery_capacity |
| battery_charge |
| battery_discharge |
| battery_health |
| cpu_cache |
| cpu_floating_point_accuracy |
| cpu_prime_search |
| cpu_stress |
| disk_read |
| dns_resolution |
| memory |
| smartctl_check |
| lan_connectivity |
| signal_strength |
| dns_resolver_present |
| gateway_can_be_pinged |
| sensitive_sensor |
| nvme_self_test |
| fingerprint_alive |
| smartctl_check_with_percentage_used |
| emmc_lifetime |
| bluetooth_power |
| ufs_lifetime |
| power_button |
| audio_driver |
| bluetooth_discovery |
| bluetooth_scanning |
| bluetooth_pairing |
| fan |

### Enum RoutineStatus
| Property Name |
------------ |
| unknown |
| ready |
| running |
| waiting_user_action |
| passed |
| failed |
| error |
| cancelled |
| failed_to_start |
| removed |
| cancelling |
| unsupported |
| not_run |

### Enum RoutineCommandType
| Property Name |
------------ |
| cancel |
| remove |
| resume |
| status |

### Enum UserMessageType
| Property Name |
------------ |
| unknown |
| unplug_ac_power |
| plug_in_ac_power |
| press_power_button |

### Enum DiskReadRoutineType
| Property Name |
------------ |
| linear |
| random |

### Enum AcPowerStatus
| Property Name |
------------ |
| connected |
| disconnected |

### Enum NvmeSelfTestType
| Property Name |
------------ |
| short_test |
| long_test |

### RunAcPowerRoutineRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| expected_status | AcPowerStatus | The expected status of the AC ('connected', 'disconnected' or 'unknown') |
| expected_power_type* | string | If specified, this must match the type of power supply for the routine to succeed. |

### RunBatteryDischargeRoutineRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| length_seconds | number | Length of time to run the routine for |
| maximum_discharge_percent_allowed | number | The routine will fail if the battery discharges by more than this percentage |

### RunBatteryChargeRoutineRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| length_seconds | number | Length of time to run the routine for |
| minimum_charge_percent_required | number | The routine will fail if the battery charges by less than this percentage |

### RunCpuRoutineRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| length_seconds | number | Length of time to run the routine for |

### RunDiskReadRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| type | DiskReadRoutineType | Type of disk read routine that will be started |
| length_seconds | number | Length of time to run the routine for |
| file_size_mb | number | test file size, in mega bytes, to test with DiskRead routine. Maximum file size is 10 GB |

### RunPowerButtonRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| timeout_seconds | number | A timeout for the routine |

### RunNvmeSelfTestRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| test_type | NvmeSelfTestType | Selects between a "short_test" or a "long_test". |

### RunSmartctlCheckRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| percentage_used_threshold | number | an optional threshold number in percentage, range [0, 255] inclusive, that the routine examines `percentage_used` against. If not specified, the routine will default to the max allowed value (255). |

### RunRoutineResponse
| Property Name | Type | Description |
------------ | ------- | ----------- |
| id | number | Id of the routine routine created |
| status | RoutineStatus | Current routine status  |

### GetRoutineUpdateRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| id | number | Id of the routine you want to query |
| command | RoutineCommandType | What kind of updated should be performed |

### GetRoutineUpdateResponse
| Property Name | Type | Description |
------------ | ------- | ----------- |
| progress_percent | number | Current progress of the routine |
| output | string | Optional debug output |
| status | RoutineStatus | Current routine status |
| status_message | string | Optional routine status message |
| user_message | UserMessageType | Returned for routines that require user action (e.g. unplug power cable) |

### GetAvailableRoutinesResponse
| Property Name | Type | Description |
------------ | ------- | ----------- |
| routines | Array<RoutineType\> | Available routine types |


## Functions

### getAvailableRoutines()
```
chrome.os.diagnostics.getAvailableRoutines() => Promise<GetAvailableRoutinesResponse>
```

#### Released in Chrome version
M96

#### Required permission
*   `os.diagnostics`

### getRoutineUpdate()
```
chrome.os.diagnostics.getRoutineUpdate(
  request: GetRoutineUpdateRequest,
) => Promise<GetRoutineUpdateResponse>
```

#### Released in Chrome version
M96

#### Required permission
*   `os.diagnostics`

### runAcPowerRoutine()
```
chrome.os.diagnostics.runAcPowerRoutine(
  request: RunAcPowerRoutineRequest,
) => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M105

#### Required permission
*   `os.diagnostics`

### runAudioDriverRoutine()
```
chrome.os.diagnostics.runAudioDriverRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M117

#### Required permission
*   `os.diagnostics`

### runBatteryCapacityRoutine()
```
chrome.os.diagnostics.runBatteryCapacityRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M96

#### Required permission
*   `os.diagnostics`



### runBatteryChargeRoutine()
```
chrome.os.diagnostics.runBatteryChargeRoutine(
  request: RunBatteryChargeRoutineRequest,
) => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M96

#### Required permission
*   `os.diagnostics`

### runBatteryDischargeRoutine()
```
chrome.os.diagnostics.runBatteryDischargeRoutine(
  request: RunBatteryDischargeRoutineRequest,
) => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M96

#### Required permission
*   `os.diagnostics`

### runBatteryHealthRoutine()
```
chrome.os.diagnostics.runBatteryHealthRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M96

#### Required permission
*   `os.diagnostics`

### runBluetoothDiscoveryRoutine()
```
chrome.os.diagnostics.runBluetoothDiscoveryRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M118

#### Required permission
*   `os.diagnostics`

### runBluetoothPairingRoutine()
```
chrome.os.diagnostics.runBluetoothPairingRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M118

#### Required permission
*   `os.diagnostics`
*   `os.bluetooth_peripherals_info`

### runBluetoothPowerRoutine()
```
chrome.os.diagnostics.runBluetoothPowerRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M117

#### Required permission
*   `os.diagnostics`

### runBluetoothScanningRoutine()
```
chrome.os.diagnostics.runBluetoothScanningRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M118

#### Required permission
*   `os.diagnostics`
*   `os.bluetooth_peripherals_info`

### runCpuCacheRoutine()
```
chrome.os.diagnostics.runCpuCacheRoutine(
  request: RunCpuRoutineRequest,
) => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M96

#### Required permission
*   `os.diagnostics`

### runCpuFloatingPointAccuracyRoutine()
```
chrome.os.diagnostics.runCpuFloatingPointAccuracyRoutine(
  request: RunCpuRoutineRequest,
) => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M99

#### Required permission
*   `os.diagnostics`

### runCpuPrimeSearchRoutine()
```
chrome.os.diagnostics.runCpuPrimeSearchRoutine(
  request: RunCpuRoutineRequest,
) => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M99

#### Required permission
*   `os.diagnostics`

### runCpuStressRoutine()
```
chrome.os.diagnostics.runCpuStressRoutine(
  request: RunCpuRoutineRequest,
) => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M96

#### Required permission
*   `os.diagnostics`

### runDiskReadRoutine()
```
chrome.os.diagnostics.runDiskReadRoutine(
  request: RunDiskReadRequest,
) => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M102

#### Required permission
*   `os.diagnostics`

### runDnsResolutionRoutine()
```
chrome.os.diagnostics.runDnsResolutionRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M108

#### Required permission
*   `os.diagnostics`

### runDnsResolverPresentRoutine()
```
chrome.os.diagnostics.runDnsResolverPresentRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M108

#### Required permission
*   `os.diagnostics`

### runEmmcLifetimeRoutine()
```
chrome.os.diagnostics.runEmmcLifetimeRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M110

#### Required permission
*   `os.diagnostics`

### runFanRoutine()
```
chrome.os.diagnostics.runFanRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M121

#### Required permission
*   `os.diagnostics`

### runFingerprintAliveRoutine()
```
chrome.os.diagnostics.runFingerprintAliveRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M110

#### Required permission
*   `os.diagnostics`

### runGatewayCanBePingedRoutine()
```
chrome.os.diagnostics.runGatewayCanBePingedRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M108

#### Required permission
*   `os.diagnostics`

### runLanConnectivityRoutine()
```
chrome.os.diagnostics.runLanConnectivityRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M102

#### Required permission
*   `os.diagnostics`

### runMemoryRoutine()
```
chrome.os.diagnostics.runMemoryRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M96

#### Required permission
*   `os.diagnostics`

### runNvmeSelfTestRoutine()
```
chrome.os.diagnostics.runNvmeSelfTestRoutine(
  request: RunNvmeSelfTestRequest,
) => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M110

#### Required permission
*   `os.diagnostics`

### runPowerButtonRoutine()
```
chrome.os.diagnostics.runPowerButtonRoutine(
  request: RunPowerButtonRequest,
) => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M117

#### Required permission
*   `os.diagnostics`

### runSensitiveSensorRoutine()
```
chrome.os.diagnostics.runSensitiveSensorRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M110

#### Required permission
*   `os.diagnostics`

### runSignalStrengthRoutine()
```
chrome.os.diagnostics.runSignalStrengthRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M108

#### Required permission
*   `os.diagnostics`

### runSmartctlCheckRoutine()
```
chrome.os.diagnostics.runSmartctlCheckRoutine(
  request: RunSmartctlCheckRequest?,
) => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M102

Optional parameter `request` added in M110.

The parameter is only available if "smartctl_check_with_percentage_used" is
returned from `GetAvailableRoutines()`.

#### Required permission
*   `os.diagnostics`

### runUfsLifetimeRoutine()
```
chrome.os.diagnostics.runUfsLifetimeRoutine() => Promise<RunRoutineResponse>
```

#### Released in Chrome version
M117.

#### Required permission
*   `os.diagnostics`

# V2 Diagnostics API

## Types

### Enum RoutineWaitingReason
| Property Name |
------------ |
| waiting_to_be_scheduled |
| waiting_for_interaction |

### Enum ExceptionReason
| Property Name |
------------ |
| unknown |
| unexpected |
| unsupported |
| app_ui_closed |
| camera_frontend_not_opened |

### Enum MemtesterTestItemEnum
| Property Name |
------------ |
| unknown |
| stuck_address |
| compare_and |
| compare_div |
| compare_mul |
| compare_or |
| compare_sub |
| compare_xor |
| sequential_increment |
| bit_flip |
| bit_spread |
| block_sequential |
| checkerboard |
| random_value |
| solid_bits |
| walking_ones |
| walking_zeroes |
| eight_bit_writes |
| sixteen_bit_writes |

### Enum RoutineSupportStatus
| Property Name |
------------ |
| supported |
| unsupported |

### RoutineInitializedInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| uuid | string | UUID of the routine that entered this state  |

### RoutineRunningInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| uuid | string | UUID of the routine that entered this state  |
| percentage | number | Current percentage of the routine status (0-100) |

### CheckLedLitUpStateInquiry
Details regarding the inquiry to check the LED lit up state. Clients should
inspect the target LED and report its state using `CheckLedLitUpStateReply`
as the argument of `replyToRoutineInquiry`.

| Property Name | Type | Description |
------------ | ------- | ----------- |

### CheckKeyboardBacklightStateInquiry
Details regarding the inquiry to check the keyboard backlight LED state. Clients
should inspect the keyboard backlight and report its state using
`CheckKeyboardBacklightStateReply` as the argument of `replyToRoutineInquiry`.

| Property Name | Type | Description |
| ------------- | ---- | ----------- |

### RoutineInquiryUnion
This is a [union type](#Dictionary_based-union-types). Exactly one field is set.

| Property Name | Type | Description |
------------ | ------- | ----------- |
| checkLedLitUpState | CheckLedLitUpStateInquiry | See `CheckLedLitUpStateInquiry`. |
| checkKeyboardBacklightState | CheckKeyboardBacklightStateInquiry | See `CheckKeyboardBacklightStateInquiry`. |

### RoutineInteractionUnion
This is a [union type](#Dictionary_based-union-types). Exactly one field is set.

| Property Name | Type | Description |
------------ | ------- | ----------- |
| inquiry | RoutineInquiryUnion | Routine inquiries need to be replied to with the `replyToRoutineInquiry` method |

### RoutineWaitingInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| uuid | string | UUID of the routine that entered this state  |
| percentage | number | Current percentage of the routine status (0-100) |
| reason | RoutineWaitingReason | Reason why the routine waits |
| message | string | Additional information, may be used to pass instruction or explanation |
| interaction | RoutineInteractionUnion | The requested interaction. When set, clients must respond to the interaction for the routine to proceed. See `RoutineInteractionUnion` to learn about how to respond to each interaction. |

### RoutineFinishedInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| uuid | string | UUID of the routine that entered this state  |
| hasPassed | boolean | Whether the routine finished successfully |
| detail | RoutineFinishedDetailUnion | Extra details about a finished routine |

### ExceptionInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| uuid | string | UUID of the routine that entered this state  |
| reason | ExceptionReason | Reason why the routine threw an exception |
| debugMessage | string | A human readable message for debugging. Don't rely on the content because it could change anytime |

### RoutineFinishedDetailUnion
This is a [union type](#Dictionary_based-union-types). Exactly one field is set.

| Property Name | Type | Description |
------------ | ------- | ----------- |
| memory | MemoryRoutineFinishedDetail | Extra detail for a finished memory routine  |
| fan | FanRoutineFinishedDetail | Extra detail for a finished fan routine |
| cameraFrameAnalysis | CameraFrameAnalysisRoutineFinishedDetail | Extra detail for a finished camera frame analysis routine |

### MemtesterResult
| Property Name | Type | Description |
------------ | ------- | ----------- |
| passedItems | Array<MemtesterTestItemEnum\> | Passed test items |
| failedItems | Array<MemtesterTestItemEnum\> | Failed test items |

### LegacyMemtesterResult
| Property Name | Type | Description |
------------ | ------- | ----------- |
| passed_items | Array<MemtesterTestItemEnum\> | Passed test items |
| failed_items | Array<MemtesterTestItemEnum\> | Failed test items |

### MemoryRoutineFinishedDetail
| Property Name | Type | Description |
------------ | ------- | ----------- |
| bytesTested | number | Number of bytes tested in the memory routine |
| result | MemtesterResult | Contains the memtester test results |

### LegacyMemoryRoutineFinishedInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| uuid | string | UUID of the routine that entered this state  |
| has_passed | boolean | Whether the routine finished successfully |
| bytesTested | number | Number of bytes tested in the memory routine |
| result | LegacyMemtesterResult | Contains the memtester test results |

### CreateMemoryRoutineArguments
| Property Name | Type | Description |
------------ | ------- | ----------- |
| maxTestingMemKib | number | An optional field to indicate how much memory should be tested. If the value is null, memory test will run with as much memory as possible |

### Enum HardwarePresenceStatus
| Property Name |
------------ |
| matched |
| not_matched |
| not_configured |

### FanRoutineFinishedDetail
| Property Name | Type | Description |
------------ | ------- | ----------- |
| passedFanIds | Array<number\> | The ids of fans that can be controlled |
| failedFanIds | Array<number\> | The ids of fans that cannot be controlled |
| fanCountStatus | HardwarePresenceStatus | Whether the number of fan probed is matched |

### LegacyFanRoutineFinishedInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| uuid | string | UUID of the routine that entered this state  |
| has_passed | boolean | Whether the routine finished successfully |
| passed_fan_ids | Array<number\> | The ids of fans that can be controlled |
| failed_fan_ids | Array<number\> | The ids of fans that cannot be controlled |
| fan_count_status | HardwarePresenceStatus | Whether the number of fan probed is matched |

### CreateFanRoutineArguments
| Property Name | Type | Description |
------------ | ------- | ----------- |

### Enum VolumeButtonType
| Property Name |
------------ |
| volume_up |
| volume_down |

### LegacyVolumeButtonRoutineFinishedInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| uuid | string | UUID of the routine that entered this state  |
| has_passed | boolean | Whether the routine finished successfully |

### LegacyCreateVolumeButtonRoutineArguments
| Property Name | Type | Description |
------------ | ------- | ----------- |
| button_type | VolumeButtonType | The volume button to be tested |
| timeout_seconds | number | Length of time to listen to the volume button events. The value should be positive and less or equal to 600 seconds |

### CreateVolumeButtonRoutineArguments
| Property Name | Type | Description |
------------ | ------- | ----------- |
| buttonType | VolumeButtonType | The volume button to be tested |
| timeoutSeconds | number | Length of time to listen to the volume button events. The value should be positive and less or equal to 600 seconds |

### CreateNetworkBandwidthRoutineArguments

Checks the network bandwidth and reports the speed info.

This routine is supported when `oem-name` in cros-config is set and not empty
string. The external service for the routine is not available for the
unrecognized devices.

| Property Name | Type | Description |
------------ | ------- | ----------- |

### Enum LedName
| Property Name |
------------ |
| battery |
| power |
| adapter |
| left |
| right |

### Enum LedColor
| Property Name |
------------ |
| red |
| green |
| blue |
| yellow |
| white |
| amber |

### CreateLedLitUpRoutineArguments
The routine lights up the target LED in the specified color and requests
the caller to verify the change.

This routine is supported if and only if the device has a ChromeOS EC.

When an LED name or LED color is not supported by the EC, it will cause a
routine exception (by emitting an `onRoutineException` event) at runtime.

The routine proceeds with the following steps:
1. Set the specified LED with the specified color and enter the waiting
   state with the `CheckLedLitUpStateInquiry` interaction request.W
2. After receiving `CheckLedLitUpStateReply` with the observed LED state,
   the color of the LED will be reset (back to auto control). Notice that
   there is no timeout so the routine will be in the waiting state
   indefinitely.
3. The routine passes if the LED is lit up in the correct color. Otherwise,
   the routine fails.

| Property Name | Type | Description |
------------ | ------- | ----------- |
| name | LedName | The LED to be lit up |
| color | LedColor | The color to be lit up |

### CreateCameraFrameAnalysisRoutineArguments
The routine checks the frames captured by camera. The frontend should ensure the
camera is opened during the execution of the routine.

| Property Name | Type | Description |
------------ | ------- | ----------- |

### CreateKeyboardBacklightRoutineArguments
This routine checks whether the keyboard backlight can be lit up at any
brightness level.

| Property Name | Type | Description |
| ------------- | ---- | ----------- |

### Enum CameraFrameAnalysisIssue
| Property Name |
------------ |
| no_issue |
| camera_service_not_available |
| blocked_by_privacy_shutter |
| lens_are_dirty |

### Enum CameraSubtestResult
| Property Name |
------------ |
| not_run |
| passed |
| failed |

### CameraFrameAnalysisRoutineFinishedDetail
| Property Name | Type | Description |
------------ | ------- | ----------- |
| issue | CameraFrameAnalysisIssue | The issue caught by the routine. See the fields for each subtest for their details. |
| privacyShutterOpenTest | CameraSubtestResult | The result is `failed` if the len is blocked by the privacy shutter. To mitigate the issue, users are suggested to open the privacy shutter to unveil the len. |
| lensNotDirtyTest | CameraSubtestResult | The result is `failed` if the frames are blurred. To mitigate the issue, users are suggested to clean the lens. |

### CreateRoutineArgumentsUnion
This is a [union type](#Dictionary_based-union-types). Exactly one field is set.

| Property Name | Type | Released in Chrome version | Description | Additional permission needed to access |
------------ | ------- | ----------- | ----------- | ----------- |
| memory | CreateMemoryRoutineArguments | M125 | Arguments to create a memory routine | None |
| volumeButton | CreateVolumeButtonRoutineArguments | M125 | Arguments to create a volume button routine | None |
| fan | CreateFanRoutineArguments | M125 | Arguments to create a fan routine | None |
| networkBandwidth | CreateNetworkBandwidthRoutineArguments | M125 | Arguments to create a network bandwidth routine | `os.diagnostics.network_info_mlab` |
| ledLitUp | CreateLedLitUpRoutineArguments | M125 | Arguments to create a LED lit up routine | None |
| cameraFrameAnalysis | CreateCameraFrameAnalysisRoutineArguments | M129 | Arguments to create a camera frame analysis routine | None |
| keyboardBacklight | CreateKeyboardBacklightRoutineArguments | M128 | Arguments to create a keyboard backlight routine | None |


### CreateRoutineResponse
| Property Name | Type | Description |
------------ | ------- | ----------- |
| uuid | string | UUID of the routine that was just created  |

### RoutineSupportStatusInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| status | RoutineSupportStatus | Whether a routine with the provided arguments is supported or unsupported |

### StartRoutineRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| uuid | string | UUID of the routine that shall be created |

### Enum LedLitUpState
| Property Name |
------------ |
| correct_color |
| not_lit_up |

### Enum KeyboardBacklightState
| Property Name |
------------ |
| ok |
| any_not_lit_up |

### CheckLedLitUpStateReply
| Property Name | Type | Description |
------------ | ------- | ----------- |
| state | LedLitUpState | State of the target LED |

### CheckKeyboardBacklightStateReply
| Property Name | Type                   | Description                         |
| ------------- | ---------------------- | ----------------------------------- |
| state         | KeyboardBacklightState | State of the keyboard backlight LED |

### RoutineInquiryReplyUnion
This is a [union type](#Dictionary_based-union-types). Exactly one field is set.

| Property Name | Type | Description |
------------ | ------- | ----------- |
| uuid | string | UUID of the routine that shall be replied |
| checkLedLitUpState | CheckLedLitUpStateReply | Reply to a `CheckLedLitUpStateInquiry` |
| checkKeyboardBacklightState | CheckKeyboardBacklightStateReply | Reply to a `CheckKeyboardBacklightStateInquiry` |

### ReplyToRoutineInquiryRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| uuid | string | UUID of the routine that shall be replied |
| reply | RoutineInquiryReplyUnion | Reply to an inquiry in the routine waiting info |

### CancelRoutineRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| uuid | string | UUID of the routine that shall be cancelled |

## Functions

### cancelRoutine()
```
chrome.os.diagnostics.cancelRoutine(
  request: CancelRoutineRequest,
) => Promise<void>
```

Stops executing the routine identified by `UUID` and removes all related
resources from the system.

#### Released in Chrome version
M119

#### Required permission
*   `os.diagnostics`

### createRoutine()
```
chrome.os.diagnostics.createRoutine(
  args: CreateRoutineArgumentsUnion,
) => Promise<CreateRoutineResponse>
```

Create a routine with `CreateRoutineArgumentsUnion`. Exactly one routine should
be set in `CreateRoutineArgumentsUnion`.

#### Released in Chrome version
M125

#### Required permission
*   `os.diagnostics`

### isRoutineArgumentSupported()
```
chrome.os.diagnostics.isRoutineArgumentSupported(
  args: CreateRoutineArgumentsUnion,
) => Promise<RoutineSupportStatusInfo>
```

Checks whether a certain `CreateRoutineArgumentsUnion` is supported on the
platform. Exactly one routine should be set in `CreateRoutineArgumentsUnion`.

#### Released in Chrome version
M125

#### Required permission
*   `os.diagnostics`

### replyToRoutineInquiry()
```
chrome.os.diagnostics.replyToRoutineInquiry(
  request: ReplyToRoutineInquiryRequest,
) => Promise<void>
```

Replies to a routine inquiry. This can only work when the routine with `UUID` is
in the waiting state and has set an inquiry in the waiting info.

#### Released in Chrome version
M125

#### Required permission
*   `os.diagnostics`

### startRoutine()
```
chrome.os.diagnostics.startRoutine(
  request: StartRoutineRequest,
) => Promise<void>
```

Starts execution of a routine. This can only be expected to work after the
`onRoutineInitialized` event was emitted for the routine with `UUID`.

#### Released in Chrome version
M119

#### Required permission
*   `os.diagnostics`

### (Deprecated) createFanRoutine()
```
chrome.os.diagnostics.createFanRoutine(
  args: CreateFanRoutineArguments,
) => Promise<CreateRoutineResponse>
```

Create a fan routine.

#### Released in Chrome version
M121

Deprecated in M125, use `createRoutine()` with a `fan` arg.

#### Required permission
*   `os.diagnostics`

### (Deprecated) createMemoryRoutine()
```
chrome.os.diagnostics.createMemoryRoutine(
  args: CreateMemoryRoutineArguments,
) => Promise<CreateRoutineResponse>
```

Create a memory routine.

#### Released in Chrome version
M119

Deprecated in M125, use `createRoutine()` with a `memory` arg.

#### Required permission
*   `os.diagnostics`

### (Deprecated) createVolumeButtonRoutine()
```
chrome.os.diagnostics.createVolumeButtonRoutine(
  args: LegacyCreateVolumeButtonRoutineArguments,
) => Promise<CreateRoutineResponse>
```

Create a volume button routine.

#### Released in Chrome version
M121

Deprecated in M125, use `createRoutine()` with a `volumeButton` arg.

#### Required permission
*   `os.diagnostics`

### (Deprecated) isFanRoutineArgumentSupported()
```
chrome.os.diagnostics.isFanRoutineArgumentSupported(
  args: CreateFanRoutineArguments,
) => Promise<RoutineSupportStatusInfo>
```

Checks whether a certain `CreateFanRoutineArguments` is supported on the
platform.

#### Released in Chrome version
M121

Deprecated in M125, use `isRoutineArgumentSupported()` with a `fan` arg.

#### Required permission
*   `os.diagnostics`

### (Deprecated) isMemoryRoutineArgumentSupported()
```
chrome.os.diagnostics.isMemoryRoutineArgumentSupported(
  args: CreateMemoryRoutineArguments,
) => Promise<RoutineSupportStatusInfo>
```

Checks whether a certain `CreateMemoryRoutineArguments` is supported on the
platform.

#### Released in Chrome version
M119

Deprecated in M125, use `isRoutineArgumentSupported()` with a `memory` arg.

#### Required permission
*   `os.diagnostics`

### (Deprecated) isVolumeButtonRoutineArgumentSupported()
```
chrome.os.diagnostics.isVolumeButtonRoutineArgumentSupported(
  args: LegacyCreateVolumeButtonRoutineArguments,
) => Promise<RoutineSupportStatusInfo>
```

Checks whether a certain `LegacyCreateVolumeButtonRoutineArguments` is supported
on the platform.

#### Released in Chrome version
M121

Deprecated in M125, use `isRoutineArgumentSupported()` with a `volumeButton`
arg.

#### Required permission
*   `os.diagnostics`

## Events

### onRoutineException
```
chrome.os.diagnostics.onRoutineException(
  function(ExceptionInfo),
)
```

Fired when an exception occurs. The error passed in `ExceptionInfo` is
non-recoverable.

#### Released in Chrome version
M119

#### Required permission
*   `os.diagnostics`

### onRoutineFinished
```
chrome.os.diagnostics.onRoutineFinished(
  function(RoutineFinishedInfo),
)
```

Fired when a routine finishes.

#### Released in Chrome version
M125

#### Required permission
*   `os.diagnostics`

### onRoutineInitialized
```
chrome.os.diagnostics.onRoutineInitialized(
  function(RoutineInitializedInfo),
)
```

Fired when a routine is initialized.

#### Released in Chrome version
M119

#### Required permission
*   `os.diagnostics`

### onRoutineRunning
```
chrome.os.diagnostics.onRoutineRunning(
  function(RoutineRunningInfo),
)
```

Fired when a routine starts running. This can happen in two situations:
1.  `startRoutine` was called and the routine successfully started execution.
2.  The routine exited the "waiting" state and returned to running.

#### Released in Chrome version
M119

#### Required permission
*   `os.diagnostics`

### onRoutineWaiting
```
chrome.os.diagnostics.onRoutineWaiting(
  function(RoutineWaitingInfo),
)
```

Fired when a routine stops execution and waits for an action, for example, user
interaction. `RoutineWaitingInfo` contains information about what the routine is
waiting for.

#### Released in Chrome version
M119

#### Required permission
*   `os.diagnostics`

### (Deprecated) onFanRoutineFinished
```
chrome.os.diagnostics.onFanRoutineFinished(
  function(LegacyFanRoutineFinishedInfo),
)
```

Fired when a fan routine finishes.

#### Released in Chrome version
M121

Deprecated in M125, use `onRoutineFinished`.

#### Required permission
*   `os.diagnostics`

### (Deprecated) onMemoryRoutineFinished
```
chrome.os.diagnostics.onMemoryRoutineFinished(
  function(LegacyMemoryRoutineFinishedInfo),
)
```

Fired when a memory routine finishes.

#### Released in Chrome version
M119

Deprecated in M125, use `onRoutineFinished`.

#### Required permission
*   `os.diagnostics`

### (Deprecated) onVolumeButtonRoutineFinished
```
chrome.os.diagnostics.onVolumeButtonRoutineFinished(
  function(LegacyVolumeButtonRoutineFinishedInfo),
)
```

Fired when a volume button routine finishes.

#### Released in Chrome version
M121

Deprecated in M125, use `onRoutineFinished`.

#### Required permission
*   `os.diagnostics`

# Events

## Types

### Enum EventCategory
| Property Name |
------------ |
| audio_jack |
| lid |
| usb |
| sd_card |
| power |
| keyboard_diagnostic |
| stylus_garage |
| touchpad_button |
| touchpad_touch |
| touchpad_connected |
| touchscreen_touch |
| touchscreen_connected |
| external_display |
| stylus_touch |
| stylus_connected |

### Enum EventSupportStatus
| Property Name |
------------ |
| supported |
| unsupported |

### EventSupportStatusInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| status | EventSupportStatus | Support status of an event |

### Enum AudioJackEvent
| Property Name |
------------ |
| connected |
| disconnected |

### Enum AudioJackDeviceType
| Property Name |
------------ |
| headphone |
| microphone |

### AudioJackEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| event | AudioJackEvent | The event that occurred |
| deviceType | AudioJackDeviceType | The device type of  |

### Enum LidEvent
| Property Name |
------------ |
| closed |
| opened |

### LidEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| event | LidEvent | The event that occurred |

### Enum KeyboardConnectionType
| Property Name |
------------ |
| internal |
| usb |
| bluetooth |
| unknown |

### Enum PhysicalKeyboardLayout
| Property Name |
------------ |
| unknown |
| chrome_os |

### Enum MechanicalKeyboardLayout
| Property Name |
------------ |
| unknown |
| ansi |
| iso |
| jis |

### Enum KeyboardNumberPadPresence
| Property Name |
------------ |
| unknown |
| present |
| not_present |

### Enum KeyboardTopRowKey
| Property Name |
------------ |
| no_key |
| unknown |
| back |
| forward |
| refresh |
| fullscreen |
| overview |
| screenshot |
| screen_brightness_down |
| screen_brightness_up |
| privacy_screen_toggle |
| microphone_mute |
| volume_mute |
| volume_down |
| volume_up |
| keyboard_backlight_toggle |
| keyboard_backlight_down |
| keyboard_backlight_up |
| next_track |
| previous_track |
| play_pause |
| screen_mirror |
| delete |

### Enum KeyboardTopRightKey
| Property Name |
------------ |
| unknown |
| power |
| lock |
| control_panel |

### KeyboardInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| id | number | The number of the keyboard's /dev/input/event* node |
| connectionType | KeyboardConnectionType | The keyboard's connection type |
| name | string | The keyboard's name |
| physicalLayout | PhysicalKeyboardLayout | The keyboard's physical layout |
| mechanicalLayout | MechanicalKeyboardLayout | The keyboard's mechanical layout |
| regionCode | string | For internal keyboards, the region code of the device (from which the visual layout can be determined) |
| numberPadPresent | KeyboardNumberPadPresence | Whether the keyboard has a number pad or not |
| topRowKeys | Array<KeyboardTopRowKey\> | List of ChromeOS specific action keys in the top row. This list excludes the left-most Escape key, and right-most key (usually Power/Lock). If a keyboard has F11-F15 keys beyond the rightmost action key, they may not be included in this list (even as "none") |
| topRightKey | KeyboardTopRightKey | For CrOS keyboards, the glyph shown on the key at the far right end of the top row |
| hasAssistantKey | boolean | Only applicable on ChromeOS keyboards |

### KeyboardDiagnosticEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| keyboardInfo | KeyboardInfo | The keyboard which has been tested |
| testedKeys | Array<number\> | Keys which have been tested. It is an array of the evdev key code |
| testedTopRowKeys | Array<number\> | Top row keys which have been tested. They are positions of the key on the top row after escape (0 is leftmost, 1 is next to the right, etc.).  Generally, 0 is F1, in some fashion. NOTE: This position may exceed the length of keyboard_info->top_row_keys, for external keyboards with keys in the F11-F15 range |

### Enum UsbEvent
| Property Name |
------------ |
| connected |
| disconnected |

### UsbEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| vendor | string | Vendor name |
| name | string | The device's name |
| vid | number | Vendor ID of the device |
| pid | number | Product ID of the device |
| categories | Array<string\> | USB device categories: https://www.usb.org/defined-class-codes |
| event | UsbEvent | The event that occurred |

### Enum ExternalDisplayEvent
| Property Name |
------------ |
| connected |
| disconnected |

### ExternalDisplayEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| event | ExternalDisplayEvent | The event that occurred |
| display_info | ExternalDisplayInfo | The information related to the plugged in external display |

### Enum SdCardEvent
| Property Name |
------------ |
| connected |
| disconnected |

### SdCardEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| event | SdCardEvent | The event that occurred |

### Enum PowerEvent
| Property Name |
------------ |
| ac_inserted |
| ac_removed |
| os_suspend |
| os_resume |

### PowerEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| event | PowerEvent | The event that occurred |

### Enum StylusGarageEvent
| Property Name |
------------ |
| inserted |
| removed |

### StylusGarageEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| event | StylusGarageEvent | The event that occurred |

### Enum InputTouchButton
| Property Name |
------------ |
| left |
| middle |
| right |

### Enum InputTouchButtonState
| Property Name |
------------ |
| pressed |
| released |

### TouchpadButtonEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| button | InputTouchButton | The input button that was interacted with |
| state | InputTouchButtonState | The new state of the button |

### TouchPointInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| trackingId | number | An id to track an initiated contact throughout its life cycle |
| x | number | The x position |
| y | number | The y position |
| pressure | number | The pressure applied to the touch contact. The value ranges from 0 to `max_pressure` as defined in `TouchpadConnectedEventInfo` and `TouchscreenConnectedEventInfo` |
| touchMajor | number | The length of the longer dimension of the touch contact |
| touchMinor | number | The length of the shorter dimension of the touch contact |

### TouchpadTouchEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| touchPoints | Array<TouchPointInfo\> | The touch points reported by the touchpad |

### TouchpadConnectedEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| maxX | number | The maximum possible x position of touch points |
| maxY | number | The maximum possible y position of touch points |
| maxPressure | number | The maximum possible pressure of touch points, or 0 if pressure is not supported |
| buttons | Array<InputTouchButton\> | The supported buttons |

### TouchscreenTouchEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| touchPoints | Array<TouchPointInfo\> | The touch points reported by the touchscreen |

### TouchscreenConnectedEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| maxX | number | The maximum possible x position of touch points |
| maxY | number | The maximum possible y position of touch points |
| maxPressure | number | The maximum possible pressure of touch points, or 0 if pressure is not supported |

### StylusTouchPointInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| x | number | The x position in the cartesian XY plane. The value ranges from 0 to `max_x` as defined in `StylusConnectedEventInfo` |
| y | number | The y position in the cartesian XY plane. The value ranges from 0 to `max_y` as defined in `StylusConnectedEventInfo` |
| pressure | number | The pressure applied to the touch contact. The value ranges from 0 to `max_pressure` as defined in `StylusConnectedEventInfo` |

### StylusTouchEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| touchPoint | StylusTouchPointInfo | The info of the stylus touch point. A null touch point means the stylus leaves the contact |

### StylusConnectedEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| maxX | number | The maximum possible x position of touch points |
| maxY | number | The maximum possible y position of touch points |
| maxPressure | number | The maximum possible pressure of touch points, or 0 if pressure is not supported |

## Functions

### isEventSupported()
```
chrome.os.events.isEventSupported(
  category: EventCategory,
) => Promise<EventSupportStatusInfo>
```

Checks whether an event is supported. The information returned by this method is
valid across reboots of the device.

#### Released in Chrome version
M115

#### Required permissions
*   `os.events`

### startCapturingEvents()
```
chrome.os.events.startCapturingEvents(
  category: EventCategory,
) => Promise<void>
```

Starts capturing events for `EventCategory`. After calling this method, an
extension can expect to be updated about events through invocations of
`on<EventCategory>Event`, until either the PWA is closed or
`stopCapturingEvents()` is called. Note that an extension is only able to
subscribe to events if the PWA is currently open.

#### Released in Chrome version
M115

#### Required permissions
*   `os.events`

### stopCapturingEvents()
```
chrome.os.events.stopCapturingEvents(
  category: EventCategory,
) => Promise<void>
```

Stops capturing `EventCategory` events. This means `on<EventCategory>Event`
won't be invoked until `startCapturingEvents()` is successfully called.

#### Released in Chrome version
M115

#### Required permissions
*   `os.events`

## Events

### onAudioJackEvent
```
chrome.os.events.onAudioJackEvent(
  function(AudioJackEventInfo),
)
```

Fired when an audio device is plugged in or out.

#### Released in Chrome version
M115

#### Required permissions
*   `os.events`

### onExternalDisplayEvent
```
chrome.os.events.onExternalDisplayEvent(
  function(ExternalDisplayEventInfo),
)
```

Fired when an `ExternalDisplay` event occurs.

#### Released in Chrome version
M117

#### Required permissions
*   `os.events`

### onKeyboardDiagnosticEvent
```
chrome.os.events.onKeyboardDiagnosticEvent(
  function(KeyboardDiagnosticEventInfo),
)
```

Fired when a keyboard diagnostic has been completed in the first party
diagnostic tool.

#### Released in Chrome version
M117

#### Required permissions
*   `os.events`

### onLidEvent
```
chrome.os.events.onLidEvent(
  function(LidEventInfo),
)
```

Fired when the device lid is opened or closed.

#### Released in Chrome version
M115

#### Required permissions
*   `os.events`

### onPowerEvent
```
chrome.os.events.onPowerEvent(
  function(PowerEventInfo),
)
```

Fired when a `Power` event occurs.

#### Released in Chrome version
M117

#### Required permissions
*   `os.events`

### onSdCardEvent
```
chrome.os.events.onSdCardEvent(
  function(SdCardEventInfo),
)
```

Fired when an `SD Card` event occurs.

#### Released in Chrome version
M117

#### Required permissions
*   `os.events`

### onStylusConnectedEvent
```
chrome.os.events.onStylusConnectedEvent(
  function(StylusConnectedEventInfo),
)
```

Fired when a `Stylus Connected` event occurs.

#### Released in Chrome version
M117

#### Required permissions
*   `os.events`

### onStylusGarageEvent
```
chrome.os.events.onStylusGarageEvent(
  function(StylusGarageEventInfo),
)
```

Fired when a `Stylus Garage` event occurs.

#### Released in Chrome version
M117

#### Required permissions
*   `os.events`

### onStylusTouchEvent
```
chrome.os.events.onStylusTouchEvent(
  function(StylusTouchEventInfo),
)
```

Fired when a `Stylus Touch` event occurs.

#### Released in Chrome version
M117

#### Required permissions
*   `os.events`

### onTouchpadButtonEvent
```
chrome.os.events.onTouchpadButtonEvent(
  function(TouchpadButtonEventInfo),
)
```

Fired when a `Touchpad Button` event occurs.

#### Released in Chrome version
M117

#### Required permissions
*   `os.events`

### onTouchpadConnectedEvent
```
chrome.os.events.onTouchpadConnectedEvent(
  function(TouchpadConnectedEventInfo),
)
```

Fired when a `Touchpad Connected` event occurs.

#### Released in Chrome version
M117

#### Required permissions
*   `os.events`

### onTouchpadTouchEvent
```
chrome.os.events.onTouchpadTouchEvent(
  function(TouchpadTouchEventInfo),
)
```

Fired when a `Touchpad Touch` event occurs.

#### Released in Chrome version
M117

#### Required permissions
*   `os.events`

### onTouchscreenConnectedEvent
```
chrome.os.events.onTouchscreenConnectedEvent(
  function(TouchscreenConnectedEventInfo),
)
```

Fired when a `Touchscreen Connected` event occurs.

#### Released in Chrome version
M118

#### Required permissions
*   `os.events`

### onTouchscreenTouchEvent
```
chrome.os.events.onTouchscreenTouchEvent(
  function(TouchscreenTouchEventInfo),
)
```

Fired when a `Touchscreen Touch` event occurs.

#### Released in Chrome version
M118

#### Required permissions
*   `os.events`

### onUsbEvent
```
chrome.os.events.onUsbEvent(
  function(UsbEventInfo),
)
```

Fired when a `Usb` event occurs.

#### Released in Chrome version
M115

#### Required permissions
*   `os.events`

# Telemetry

## Types

### AudioOutputNodeInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| id | number | Node id |
| name | string | The name of this node. For example, "Speaker" |
| deviceName | string | The name of the device that this node belongs to. For example, "HDA Intel PCH: CA0132 Analog:0,0" |
| active | boolean | Whether this node is currently used for output. There is one active node for output |
| nodeVolume | number | The node volume in [0, 100] |

### AudioInputNodeInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| id | number | Node id |
| name | string | The name of this node. For example, "Internal Mic" |
| deviceName | string | The name of the device that this node belongs to. For example, "HDA Intel PCH: CA0132 Analog:0,0" |
| active | boolean | Whether this node is currently used for input. There is one active node for input |
| nodeGain | number | The input node gain set by UI, the value is in [0, 100] |

### AudioInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| outputMute | boolean | Is the active output device mute or not |
| inputMute | boolean | Is the active input device mute or not |
| underruns | number | Number of underruns |
| severeUnderruns | number | Number of severe underruns |
| outputNodes | Array<AudioOutputNodeInfo\> | Output nodes |
| inputNodes | Array<AudioInputNodeInfo\> | Input nodes |

### BatteryInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| chargeFull | number | Full capacity (Ah) |
| chargeFullDesign | number | Design capacity (Ah) |
| chargeNow | number | Battery's charge (Ah) |
| currentNow | number | Battery's current (A) |
| cycleCount | number | Battery's cycle count |
| manufactureDate | string | Manufacturing date in yyyy-mm-dd format. Included when the main battery is Smart |
| modelName | string | Battery's model name |
| serialNumber | string | Battery's serial number |
| status | string | Battery's status (e.g. charging) |
| technology | string | Used technology in the battery |
| temperature | number | Temperature in 0.1K. Included when the main battery is Smart |
| vendor | string | Battery's manufacturer |
| voltageMinDesign | number | Desired minimum output voltage |
| voltageNow | number | Battery's voltage (V) |

### NonRemovableBlockDeviceInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| name | string | The name of the block device. |
| type | string | The type of the block device, (e.g. "MMC", "NVMe" or "ATA"). |
| size | number | The device size in bytes. |

### NonRemovableBlockDeviceInfoResponse
| Property Name | Type | Description |
------------ | ------- | ----------- |
| deviceInfos | Array<NonRemovableBlockDeviceInfo\> | The list of block devices. |

### Enum CpuArchitectureEnum
| Property Name |
------------ |
| unknown |
| x86_64 |
| aarch64 |
| armv7l |

### PhysicalCpuInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| logicalCpus | Array<LogicalCpuInfo\> | Logical CPUs corresponding to this physical CPU |
| modelName | string | The CPU model name |

### LogicalCpuInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| cStates | Array<CpuCStateInfo\> | Information about the logical CPU's time in various C-states |
| idleTimeMs | number | Idle time since last boot, in milliseconds |
| maxClockSpeedKhz | number | The max CPU clock speed in kilohertz |
| scalingCurrentFrequencyKhz | number | Current frequency the CPU is running at |
| scalingMaxFrequencyKhz | number | Maximum frequency the CPU is allowed to run at, by policy |
| coreId | number | The ID of this logical CPU core |

### CpuCStateInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| name | string | State name |
| timeInStateSinceLastBootUs | number | Time spent in the state since the last reboot, in microseconds |

### CpuInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| architecture | CpuArchitectureEnum | The CPU architecture - it's assumed all of a device's CPUs share the same architecture |
| numTotalThreads | number | Number of total threads available |
| physicalCpus | Array<PhysicalCpuInfo\> | Information about the device's physical CPUs |

### Enum DisplayInputType
| Property Name |
------------ |
| unknown |
| digital |
| analog |

### EmbeddedDisplayInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| privacyScreenSupported | boolean | Whether a privacy screen is supported or not |
| privacyScreenEnabled | boolean | Whether a privacy screen is enabled or not |
| displayWidth | number | Display width in millimeters |
| displayHeight | number | Display height in millimeters |
| resolutionHorizontal | number | Horizontal resolution |
| resolutionVertical | number | Vertical resolution |
| refreshRate | number | Refresh rate |
| manufacturer | string | Three letter manufacturer ID |
| modelId | number | Manufacturer product code |
| serialNumber | number | 32 bits serial number |
| manufactureWeek | number | Week of manufacture |
| manufactureYear | number | Year of manufacture |
| edidVersion | string | EDID version |
| inputType | DisplayInputType | Digital or analog input |
| displayName | string | Name of display product |

### ExternalDisplayInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| displayWidth | number | Display width in millimeters |
| displayHeight | number | Display height in millimeters |
| resolutionHorizontal | number | Horizontal resolution |
| resolutionVertical | number | Vertical resolution |
| refreshRate | number | Refresh rate |
| manufacturer | string | Three letter manufacturer ID |
| modelId | number | Manufacturer product code |
| serialNumber | number | 32 bits serial number |
| manufacture_week | number | Week of manufacture |
| manufacture_year | number | Year of manufacture |
| edidVersion | string | EDID version |
| inputType | DisplayInputType | Digital or analog input |
| displayName | string | Name of display product |

### DisplayInfo
### ExternalDisplayInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| embeddedDisplay | EmbeddedDisplayInfo | Embedded display info |
| externalDisplays | Array<ExternalDisplayInfo\> | External display info |

### Enum NetworkType
| Property Name |
------------ |
| cellular |
| ethernet |
| tether |
| vpn |
| wifi |

### Enum NetworkState
| Property Name | description |
------------ | ------- |
| uninitialized | The network type is available but not yet initialized |
| disabled | The network type is available but disabled or disabling |
| prohibited | The network type is prohibited by policy |
| not_connected | The network type is available and enabled or enabling, but no network connection has been established |
| connecting | The network type is available and enabled, and a network connection is in progress |
| portal | The network is in a portal state |
| connected | The network is in a connected state, but connectivity is limited |
| online | The network is connected and online |

### NetworkInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| type | NetworkType | The type of network interface (wifi, ethernet, etc.) |
| state | NetworkState | The current state of the network interface (disabled, enabled, online, etc.) |
| macAddress | string | (Added in M110): The currently assigned mac address. Only available with the permission os.telemetry.network_info. |
| ipv4Address | string | The currently assigned ipv4Address of the interface |
| ipv6Addresses | Array<string\> | The list of currently assigned ipv6Addresses of the interface |
| signalStrength | number | The current signal strength in percent |

### InternetConnectivityInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| networks | Array<NetworkInfo\> | List of available network interfaces and their configuration |

### MarketingInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| marketingName | string | Contents of CrosConfig in `/branding/marketing-name` |

### MemoryInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| totalMemoryKiB | number | Total memory, in kilobytes |
| freeMemoryKiB | number | Free memory, in kilobytes |
| availableMemoryKiB | number | Available memory, in kilobytes |
| pageFaultsSinceLastBoot | number | Number of page faults since the last boot |

### OemDataInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| oemData | string | OEM's specific data. This field is used to store battery serial number by some OEMs |

### OsVersionInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| releaseMilestone | string | The release milestone (e.g. "87") |
| buildNumber | string | The build number (e.g. "13544") |
| patchNumber | string | The build number (e.g. "59.0") |
| releaseChannel | string | The release channel (e.g. "stable-channel") |

### VpdInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| skuNumber | string | Device's SKU number, a.k.a. product number |
| serialNumber | string | Device's serial number |
| modelName | string | Device's model name |
| activateDate | string | Device's activate date: Format: YYYY-WW |

### StatefulPartitionInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| availableSpace | number | The currently available space in the user partition (Bytes) |
| totalSpace | number | The total space of the user partition (Bytes) |

### Enum ThermalSensorSource
| Property Name |
------------ |
| unknown |
| ec |
| sysFs |

### ThermalSensorInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| name | string | Name of the thermal sensor  |
| temperatureCelsius | number | Temperature detected by the thermal sensor in celsius |
| source | ThermalSensorSource | Where the thermal sensor is detected from  |

### ThermalInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| thermalSensors | Array<ThermalSensorInfo\> | An array containing all the information retrieved for thermal sensors |

### Enum TpmGSCVersion
| Property Name |
------------ |
| not_gsc |
| cr50 |
| ti5 |

### TpmVersion
| Property Name | Type | Description |
------------ | ------- | ----------- |
| gscVersion | TpmGSCVersion | The version of Google security chip(GSC), or "not_gsc" if not applicable  |
| family | number | TPM family. We use the TPM 2.0 style encoding (see [here](https://trustedcomputinggroup.org/wp-content/uploads/TPM-Rev-2.0-Part-1-Architecture-01.07-2014-03-13.pdf)  for reference), e.g.: <ul><li>TPM 1.2: "1.2" -> 0x312e3200</li><li> TPM 2.0: "2.0" -> 0x322e3000</li></ul> |
| specLevel | number | The level of the specification that is implemented by the TPM  |
| manufacturer | number | A manufacturer specific code |
| tpmModel | number | The TPM model number |
| firmwareVersion | number | The current firmware version of the TPM  |
| vendorSpecific | string | Information set by the vendor |

### TpmStatus
| Property Name | Type | Description |
------------ | ------- | ----------- |
| enabled | boolean | Whether the |
| owned | boolean | Whether the TPM has been owned |
| specLevel | boolean | Whether the owner password is still retained (as part of the TPM initialization) |

### TpmDictionaryAttack
| Property Name | Type | Description |
------------ | ------- | ----------- |
| counter | number | The current dictionary attack counter value |
| threshold | number | The current dictionary attack counter threshold |
| lockoutInEffect | boolean | Whether the TPM is currently in some form of dictionary attack lockout |
| lockoutSecondsRemaining | number | The number of seconds remaining in the lockout (if applicable) |

### TpmInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| version | TpmVersion | The current version of the Trusted Platform Module (TPM) |
| status | TpmStatus | The current status of the TPM |
| dictionaryAttack | TpmDictionaryAttack | TPM dictionary attack (DA) related information |

### UsbBusInterfaceInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| interfaceNumber | number | The zero-based number (index) of the interface |
| classId | number | The class id can be used to classify / identify the usb interfaces. See the usb.ids database for the values (https://github.com/gentoo/hwids) |
| subclassId | number | The subclass id can be used to classify / identify the usb interfaces. See the usb.ids database for the values (https://github.com/gentoo/hwids) |
| protocolId | number | The protocol id can be used to classify / identify the usb interfaces. See the usb.ids database for the values (https://github.com/gentoo/hwids) |
| driver | string | The driver used by the device. This is the name of the matched driver which is registered in the kernel. See "{kernel root}/drivers/" for the list of the built in drivers |

### Enum FwupdVersionFormat
| Property Name | Description |
------------ | ------------- |
| plain | An unidentified format text string |
| number | A single integer version number |
| pair | Two AABB.CCDD version numbers |
| triplet | Microsoft-style AA.BB.CCDD version numbers |
| quad | UEFI-style AA.BB.CC.DD version numbers |
| bcd | Binary coded decimal notation |
| intelMe | Intel ME-style bitshifted notation |
| intelMe2 | Intel ME-style A.B.CC.DDDD notation |
| surfaceLegacy | Legacy Microsoft Surface 10b.12b.10b |
| surface | Microsoft Surface 8b.16b.8b |
| dellBios | Dell BIOS BB.CC.DD style |
| hex | Hexadecimal 0xAABCCDD style |

### FwupdFirmwareVersionInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| version | string | The string form of the firmware version |
| version_format | FwupdVersionFormat | The format for parsing the version string |

### Enum UsbVersion
| Property Name |
------------ |
| unknown |
| usb1 |
| usb2 |
| usb3 |

### Enum UsbSpecSpeed
An enumeration of the usb spec speed in Mbps.
Source:

1. https://www.kernel.org/doc/Documentation/ABI/testing/sysfs-bus-usb
2. https://www.kernel.org/doc/Documentation/ABI/stable/sysfs-bus-usb
3. https://en.wikipedia.org/wiki/USB

| Property Name | Description |
------------ | ------------- |
| unknown | Unknown speed |
| n1_5Mbps | Low speed |
| n12Mbps | Full speed |
| n480Mbps | High Speed |
| n5Gbps | Super Speed |
| n10Gbps | Super Speed+ |
| n20Gbps | Super Speed+ Gen 2x2 |

### UsbBusInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| classId | number | The class id can be used to classify / identify the usb interfaces. See the usb.ids database for the values (https://github.com/gentoo/hwids) |
| subclassId | number | The subclass id can be used to classify / identify the usb interfaces. See the usb.ids database for the values (https://github.com/gentoo/hwids) |
| protocolId | number | The protocol id can be used to classify / identify the usb interfaces. See the usb.ids database for the values (https://github.com/gentoo/hwids) |
| vendorId | number | The vendor id can be used to classify / identify the usb interfaces. See the usb.ids database for the values (https://github.com/gentoo/hwids) |
| productId | number | The product id can be used to classify / identify the usb interfaces. See the usb.ids database for the values (https://github.com/gentoo/hwids) |
| interfaces | Array<UsbBusInterfaceInfo\> | The usb interfaces under the device. A usb device has at least one interface. Each interface may or may not work independently, based on each device. This allows a usb device to provide multiple features. The interfaces are sorted by the `interface_number` field |
| fwupdFirmwareVersionInfo | FwupdFirmwareVersionInfo | The firmware version obtained from fwupd |
| version | UsbVersion | The recognized usb version. It may not be the highest USB version supported by the hardware |
| spec_speed | UsbSpecSpeed | The spec usb speed |

### UsbDevicesInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| devices | Array<UsbBusInfo\> | Information about all connected USB devices |

## Functions

### getAudioInfo()
```
chrome.os.telemetry.getAudioInfo() => Promise<AudioInfo>
```

#### Released in Chrome version
M111

#### Required permissions
*   `os.telemetry`

### getBatteryInfo()
```
chrome.os.telemetry.getBatteryInfo() => Promise<BatteryInfo>
```

#### Released in Chrome version
M102

#### Required permissions
*   `os.telemetry`
*   `os.telemetry.serial_number` for serial number field

### getCpuInfo()
```
chrome.os.telemetry.getCpuInfo() => Promise<CpuInfo>
```

#### Released in Chrome version
M96

#### Required permissions
*   `os.telemetry`

### getDisplayInfo()
```
chrome.os.telemetry.getDisplayInfo() => Promise<DisplayInfo>
```

#### Released in Chrome version
M117

#### Required permissions
*   `os.telemetry`

### getInternetConnectivityInfo()
```
chrome.os.telemetry.getInternetConnectivityInfo() => Promise<InternetConnectivityInfo>
```

#### Released in Chrome version
M108

Mac address added in M111.

#### Required permissions
*   `os.telemetry`
*   `os.telemetry.network_info` for MAC address field

### getMarketingInfo()
```
chrome.os.telemetry.getMarketingInfo() => Promise<MarketingInfo>
```

#### Released in Chrome version
M111

#### Required permissions
*   `os.telemetry`

### getMemoryInfo()
```
chrome.os.telemetry.getMemoryInfo() => Promise<MemoryInfo>
```

#### Released in Chrome version
M99

#### Required permissions
*   `os.telemetry`

### getNonRemovableBlockDevicesInfo()
```
chrome.os.telemetry.getNonRemovableBlockDevicesInfo() => Promise<NonRemovableBlockDeviceInfoResponse>
```

#### Released in Chrome version
M108

#### Required permissions
*   `os.telemetry`

### getOemData()
```
chrome.os.telemetry.getOemData() => Promise<OemDataInfo>
```

#### Released in Chrome version
M96

#### Required permissions
*   `os.telemetry`
*   `os.telemetry.serial_number`

### getOsVersionInfo()
```
chrome.os.telemetry.getOsVersionInfo() => Promise<OsVersionInfo>
```

#### Released in Chrome version
M105

#### Required permissions
*   `os.telemetry`

### getStatefulPartitionInfo()
```
chrome.os.telemetry.getStatefulPartitionInfo() => Promise<StatefulPartitionInfo>
```

#### Released in Chrome version
M105

#### Required permissions
*   `os.telemetry`

### getThermalInfo()
```
chrome.os.telemetry.getThermalInfo() => Promise<ThermalInfo>
```

#### Released in Chrome version
M122

#### Required permissions
*   `os.telemetry`

### getTpmInfo()
```
chrome.os.telemetry.getTpmInfo() => Promise<TpmInfo>
```

#### Released in Chrome version
M108

#### Required permissions
*   `os.telemetry`

### getUsbBusInfo()
```
chrome.os.telemetry.getUsbBusInfo() => Promise<UsbDevicesInfo>
```

#### Released in Chrome version
M114

#### Required permissions
*   `os.telemetry`
*   `os.attached_device_info`

### getVpdInfo()
```
chrome.os.telemetry.getVpdInfo() => Promise<VpdInfo>
```

#### Released in Chrome version
M96

#### Required permissions
*   `os.telemetry`
*   `os.telemetry.serial_number` for serial number field

# Management

## Types

### SetAudioGainArguments
| Property Name | Type | Description |
------------ | ------- | ----------- |
| nodeId | number | Node id of the audio device to be configured |
| gain | number | Target gain percent in [0, 100]. Sets to 0 or 100 if outside |

### SetAudioVolumeArguments
| Property Name | Type    | Description                                                    |
| ------------- | ------- | -------------------------------------------------------------- |
| nodeId        | number  | Node id of the audio device to be configured                   |
| volume        | number  | Target volume percent in [0, 100]. Sets to 0 or 100 if outside |
| isMuted       | boolean | Whether to mute the device                                     |

## Functions

### setAudioGain()
```
chrome.os.management.setAudioGain(
  args: SetAudioGainArguments,
) => Promise<boolean>
```

Sets the specified input audio device's gain to value. Returns false if
`args.nodeId` is invalid.

#### Released in Chrome version
M122

#### Required permissions
*   `os.management.audio`

### setAudioVolume()
```
chrome.os.management.setAudioGain(
  args: SetAudioVolumeArguments,
) => Promise<boolean>
```

Sets the specified output audio device's volume and mute state to the given
value. Returns false if `args.nodeId` is invalid.

#### Released in Chrome version
M122

#### Required permissions
*   `os.management.audio`
