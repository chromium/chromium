# Telemetry Extension API overview

This document gives a function-level documentation of the Telemetry Extension
API. It is separated in the three namespaces **telemetry**, **diagnostics**
and **events**.

[TOC]

# Diagnostics

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
| nvme_wear_level |
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

### GetRoutineUpdateRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| id | number | Id of the routine you want to query |
| command | RoutineCommandType | What kind of updated should be performed |

### RunPowerButtonRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| timeout_seconds | number | A timeout for the routine |

### GetRoutineUpdateResponse
| Property Name | Type | Description |
------------ | ------- | ----------- |
| progress_percent | number | Current progress of the routine |
| output | string | Optional debug output |
| status | RoutineStatus | Current routine status |
| status_message | string | Optional routine status message |
| user_message | UserMessageType | Returned for routines that require user action (e.g. unplug power cable) |

### RunNvmeWearLevelRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| wear_level_threshold | number | Threshold number in percentage which routine examines wear level status against |

### RunNvmeSelfTestRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| test_type | NvmeSelfTestType | Selects between a "short_test" or a "long_test". |

### RunSmartctlCheckRequest
| Property Name | Type | Description |
------------ | ------- | ----------- |
| percentage_used_threshold | number | an optional threshold number in percentage, range [0, 255] inclusive, that the routine examines `percentage_used` against. If not specified, the routine will default to the max allowed value (255). |

## Functions

| Function Name | Definition | Permission needed to access | Released in `dpsl` version |
------------ | ------------- | ------------- | ------------- |
| getAvailableRoutines | () => Promise<List<RoutineType\>\> | `os.diagnostics` | M96 |
| getRoutineUpdate | (params: GetRoutineUpdateRequest) => Promise<GetRoutineUpdateResponse>\> | `os.diagnostics` | M96 |
| runAcPowerRoutine | (params: RunAcPowerRoutineRequest) => Promise<Routine\> | `os.diagnostics` | M105 |
| runAudioDriverRoutine | () => Promise<Routine\> | `os.diagnostics` | M117 |
| runBatteryCapacityRoutine | () => Promise<Routine\> | `os.diagnostics` | M96 |
| runBatteryHealthRoutine | () => Promise<Routine\> | `os.diagnostics` | M96 |
| runBatteryDischargeRoutine | (params: RunBatteryDischargeRoutineRequest) => Promise<Routine\> | `os.diagnostics` | M96 |
| runBatteryChargeRoutine | (params: RunBatteryChargeRoutineRequest) => Promise<Routine\> | `os.diagnostics` | M96 |
| runBluetoothPowerRoutine | () => Promise<Routine\> | `os.diagnostics` | M117 |
| runCpuCacheRoutine | (params: RunCpuRoutineRequest) => Promise<Routine\> | `os.diagnostics` | M96 |
| runCpuFloatingPointAccuracyRoutine | (params: RunCpuRoutineRequest) => Promise<Routine\> | `os.diagnostics` | M99 |
| runCpuPrimeSearchRoutine | (params: RunCpuRoutineRequest) => Promise<Routine\> | `os.diagnostics` | M99 |
| runCpuStressRoutine | (params: RunCpuRoutineRequest) => Promise<Routine\> | `os.diagnostics` | M96 |
| runDiskReadRoutine | (params: RunDiskReadRequest) => Promise<Routine\> | `os.diagnostics` | M102 |
| runDnsResolutionRoutine | () => Promise<Routine\> | `os.diagnostics` | M108 |
| runDnsResolverPresentRoutine | () => Promise<Routine\> | `os.diagnostics` | M108 |
| runEmmcLifetimeRoutine | () => Promise<Routine\> | `os.diagnostics` | M110 |
| runFingerprintAliveRoutine | () => Promise<Routine\> | `os.diagnostics` | M110 |
| runGatewayCanBePingedRoutine | () => Promise<Routine\> | `os.diagnostics` | M108 |
| runLanConnectivityRoutine | () => Promise<Routine\> | `os.diagnostics` | M102 |
| runMemoryRoutine | () => Promise<Routine\> | `os.diagnostics` | M96 |
| runNvmeSelfTestRoutine | (params: RunNvmeSelfTestRequest) => Promise<Routine\> | `os.diagnostics` | M110 |
| runNvmeWearLevelRoutine | (params: RunNvmeWearLevelRequest) => Promise<Routine\> | `os.diagnostics` | M102 |
| runPowerButtonRoutine | (params: RunPowerButtonRequest) => Promise<Routine\> | `os.diagnostics` | M117 |
| runSensitiveSensorRoutine | () => Promise<Routine\> | `os.diagnostics` | M110 |
| runSignalStrengthRoutine | () => Promise<Routine\> | `os.diagnostics` | M108 |
| runSmartctlCheckRoutine | (params: RunSmartctlCheckRequest?) => Promise<Routine\> | `os.diagnostics` | initial release: M102, new parameter added: M110. The parameter is only available if "smartctl_check_with_percentage_used" is returned from `GetAvailableRoutines` |
| runUfsLifetimeRoutine | () => Promise<Routine\> | `os.diagnostics` | M117 |

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
| button | InputTouchButtonState | The new state of the button |

### TouchPointInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| trackingId | number | An id to track an initiated contact throughout its life cycle |
| x | number | The x position |
| y | number | The y position |
| pressure | number | The pressure applied to the touch contact. The value ranges from 0 to `max_pressure` as defined in `TouchpadConnectedEventInfo` |
| touchMajor | number | The length of the longer dimension of the touch contact |
| touchMinor | number | The length of the shorter dimension of the touch contact |

### TouchpadTouchEventInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| touchPoints | Array<TouchPointInfo\> | The touch points reported by the touchpad |

### TouchpadConnectedEventInfo
| maxX | number | The maximum possible x position of touch points |
| maxY | number | The maximum possible y position of touch points |
| maxPressure | number | The maximum possible pressure of touch points, or 0 if pressure is not supported |
| buttons | Array<InputTouchButton\> | The supported buttons |

## Functions

| Function Name | Definition | Permission needed to access | Released in `dpsl` version | Description |
------------ | ------------- | ------------- | ------------- | ------------- |
| isEventSupported | (category: EventCategory) => Promise<EventSupportStatusInfo\> | `os.events` | M115 | Checks whether an event is supported. The information returned by this method is valid across reboots of the device |
| startCapturingEvents | (category: EventCategory) => () | `os.events` | M115 | Starts capturing events for `EventCategory`. After calling this method, an extension can expect to be updated about events through invocations of `on<EventCategory>Event`, until either the PWA is closed or `stopCapturingEvents` is called. Note that an extension is only able to subscribe to events if the PWA is currently open |
| stopCapturingEvents | (category: EventCategory) => () | `os.events` | M115 | Stops capturing `EventCategory` events. This means `on<EventCategory>Event` won't be invoked until `startCapturingEvents` is successfully called |

## Events

| Function Name | Definition | Permission needed to access | Released in `dpsl` version | Emitted on |
------------ | ------------- | ------------- | ------------- | ------------- |
| onAudioJackEvent | function(AudioJackEventInfo) | `os.events` | M115 | An audio device was plugged in or out |
| onKeyboardDiagnosticEvent | function(KeyboardDiagnosticEventInfo) | `os.events` | M117 | Informs the extension that a Keyboard diagnostic has been completed in the first party diagnostic tool |
| onLidEvent | function(LidEventInfo) | `os.events` | M115 | The device lid was opened or closed |
| onUsbEvent | function(UsbEventInfo) | `os.events` | M117 | Informs the extension that a `Usb` event occurred |
| onExternalDisplayEvent | function(ExternalDisplayEventInfo) | `os.events` | M117 | Informs the extension that a `ExternalDisplay` event occurred |
| onSdCardEvent | function(SdCardEventInfo) | `os.events` | M117 | Informs the extension that a `SD Card` event occurred |
| onPowerEvent | function(PowerEventInfo) | `os.events` | M117 | Informs the extension that a `Power` event occurred |
| onStylusGarageEvent | function(StylusGarageEventInfo) | `os.events` | M117 | Informs the extension that a `Stylus Garage` event occurred |
| onTouchpadButtonEvent | function(TouchpadButtonEventInfo) | `os.events` | M117 | Informs the extension that a `Touchpad Button` event occurred |
| onTouchpadTouchEvent | function(TouchpadTouchEventInfo) | `os.events` | M117 | Informs the extension that a `Touchpad Touch` event occurred |
| onTouchpadConnectedEvent | function(TouchpadConnectedEventInfo) | `os.events` | M117 | Informs the extension that a `Touchpad Connected` event occurred |
| onStylusTouchEvent | function(StylusTouchEventInfo) | `os.events` | M117 | Informs the extension that a `Stylus Touch` event occurred |
| onStylusConnectedEvent | function(StylusConnectedEventInfo) | `os.events` | M117 | Informs the extension that a `Stylus Connected` event occurred |

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
| outputNodes | Array<AudioOutputNodeInfo> | Output nodes |
| inputNodes | Array<AudioInputNodeInfo> | Input nodes |

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

### BlockDeviceInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| name | string | The name of the block device. |
| type | string | The type of the block device, (e.g. "MMC", "NVMe" or "ATA"). |
| size | number | The device size in bytes. |

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
| logicalCpus | Array<LogicalCpuInfo> | Logical CPUs corresponding to this physical CPU |
| modelName | string | The CPU model name |

### LogicalCpuInfo
| Property Name | Type | Description |
------------ | ------- | ----------- |
| cStates | Array<CpuCStateInfo> | Information about the logical CPU's time in various C-states |
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

| Function Name | Definition | Permission needed to access | Released in Chrome version |
------------ | ------------- | ------------- | ------------- |
| getVpdInfo | () => Promise<VpdInfo\> | `os.telemetry`, `os.telemetry.serial_number` for serial number field | M96 |
| getOemData | () => Promise<OemDataInfo\> | `os.telemetry`, `os.telemetry.serial_number` for the whole result | M96 |
| getCpuInfo | () => Promise<CpuInfo\> | `os.telemetry` | M99 |
| getDisplayInfo | () => Promise<DisplayInfo\> | `os.telemetry` | M117 |
| getMemoryInfo | () => Promise<MemoryInfo\> | `os.telemetry` | M99 |
| getBatteryInfo | () => Promise<BatteryInfo\> | `os.telemetry`, `os.telemetry.serial_number` for serial number field | M102 |
| getStatefulPartitionInfo | () => Promise<StatefulPartitionInfo\> | `os.telemetry` | M105 |
| getOsVersionInfo | () => Promise<OsVersionInfo\> | `os.telemetry` | M105 |
| getNonRemovableBlockDevicesInfo | () => Promise<BlockDeviceInfo\> | `os.telemetry` | M108 |
| getInternetConnectivityInfo | () => Promise<InternetConnectivityInfo\> | `os.telemetry`, `os.telemetry.network_info` for MAC address field | M108 - Mac address in M111 |
| getTpmInfo | () => Promise<TpmInfo\> | `os.telemetry` | M108 |
| getAudioInfo | () => Promise<AudioInfo\> | `os.telemetry` | M111 |
| getMarketingInfo | () => Promise<MarketingInfo\> | `os.telemetry` | M111 |
| getUsbBusInfo | () => Promise<UsbDevicesInfo\> | `os.telemetry`, `os.attached_device_info` | M114 |
