# DPSL.js Migration Guide

# Objective

As we are encouraging OEMs to call Chrome Extension APIs directly instead of via
DPSL.js, this document provides an overview of what is changing for OEMs. In
most cases this only requires a method call replacement, since DPSL.js is a thin
wrapper around our Chrome Extension APIs.

[TOC]

# The telemetry namespace (dpsl.telemetry)

Calls in the `chrome.os.telemetry` namespace are a 1:1 mapping from dpsl APIs to
extension API calls. The only difference is that dpsl catches the case of
calling an `undefined` API (assuming it is undefined because it is not yet
released) and throws a
[`MethodNotFoundError`](https://github.com/GoogleChromeLabs/telemetry-support-extension-for-chromeos/blob/main/src/utils.js#L47-L60).

The table below shows the mapping from dpsl calls to Chrome Extension calls, the
returned data is equivalent and the chrome extension APIs are also
asynchronous.

| **DPSL function** | **Chrome Extension function** |
| ----------------- | ----------------------------- |
| dpsl.telemetry.getVpdInfo | chrome.os.telemetry.getVpdInfo |
| dpsl.telemetry.getOemData | chrome.os.telemetry.getOemData |
| dpsl.telemetry.getCpuInfo | chrome.os.telemetry.getCpuInfo |
| dpsl.telemetry.getMemoryInfo | chrome.os.telemetry.getMemoryInfo |
| dpsl.telemetry.getBatteryInfo | chrome.os.telemetry.getBatteryInfo |
| dpsl.telemetry.getStatefulPartitionInfo | chrome.os.telemetry.getStatefulPartitionInfo |
| dpsl.telemetry.getOsVersionInfo | chrome.os.telemetry.getOsVersionInfo |
| dpsl.telemetry.getNonRemovableBlockDevicesInfo | chrome.os.telemetry.getNonRemovableBlockDevicesInfo |
| dpsl.telemetry.getInternetConnectivityInfo | chrome.os.telemetry.getInternetConnectivityInfo |
| dpsl.telemetry.getTpmInfo | chrome.os.telemetry.getTpmInfo |
| dpsl.telemetry.getAudioInfo | chrome.os.telemetry.getAudioInfo |
| dpsl.telemetry.getMarketingInfo | chrome.os.telemetry.getMarketingInfo |
| dpsl.telemetry.getUsbBusInfo | chrome.os.telemetry.getUsbBusInfo |

# The diagnostics namespace (dpsl.diagnostics)

Similar to the telemetry namespace, functions in the diagnostics namespace also
map 1:1 to Chrome Extension functions. The only difference is, that dpsl APIs
return a
[`Routine`](https://github.com/GoogleChromeLabs/telemetry-support-extension-for-chromeos/blob/main/src/diagnostics_manager.js#L40-L97)
object that wraps Chrome Extension APIs for handling diagnostics nicely. OEMs
can copy the
[definition](https://github.com/GoogleChromeLabs/telemetry-support-extension-for-chromeos/blob/main/src/diagnostics_manager.js#L40-L97)
of this wrapper class to their own codebase to have the same experience as with
using `dpsl.Routine`.

Alternatively, OEMs can also interact directly with Chrome Extension calls to
control a routine. The return value of every
`chrome.os.diagnostics.runXYZRoutine` is an object that defines the following
fields:

```
// Will be represented as a string in JavaScript, e.g. "ready".
enum RoutineStatus {
  unknown,
  ready,
  running,
  waiting_user_action,
  passed,
  failed,
  error,
  cancelled,
  failed_to_start,
  removed,
  cancelling,
  unsupported,
  not_run
};

// Returned from `chrome.os.runXYZRoutine`.
dictionary RunRoutineResponse {
  // The id will turn into a `number` in JavaScript.
  long id;
  RoutineStatus status;
};
```

The `RoutineStatus` tells the OEMs whether the request to start a routine was
successful. If the status is "running", one can assume that the routine has been
started successfully. However, for routines that complete very quickly, it is
possible that the routine will enter a final state ("passed", "failed", or
"error") before the "running" state is observed. The id is a unique identifier
used for tracking a specific routine over its lifetime.

To interact with a started routine, OEMs can use the
`chrome.os.diagnostics.getRoutineUpdate` function. It takes an
`RoutineUpdateRequest` object as a parameter and returns a
`GetRoutineUpdateResponse` object. The two objects are defined as follows:

```
enum RoutineCommandType {
  cancel,
  remove,
  resume,
  status
};

dictionary GetRoutineUpdateRequest {
  long id;
  RoutineCommandType command;
};

enum UserMessageType {
  unknown,
  unplug_ac_power,
  plug_in_ac_power
};

dictionary GetRoutineUpdateResponse {
  long progress_percent;
  // The question mark represents an optional value.
  DOMString? output;
  // The routine status is the same as above.
  RoutineStatus status;
  DOMString status_message;
  // Returned for routines that require user action (e.g. unplug power cable), the
  // question mark represents an optional value.
  UserMessageType? user_message;
};
```

The following operations can be done via calling
`chrome.os.diagnostics.getRoutineUpdate`, all represented by a different
`RoutineCommandType`:

-  Resume a currently waiting routine (e.g. after it needed user input)
    with "resume"
-  Cancel a running routine with "cancel"
-  Get the status of a routine with "status"
-  Remove a routine (and release its allocated resources) with “remove”

**Note: OEMs need to make sure to properly remove routines they manually stopped,
since cancel doesn't include removal and doesn't deallocate resources. After
canceling a routine it should always be removed.**

As mentioned above the `dpsl.Routine` object encapsulates all this functionality
into three methods:

-  getStatus: Calling routine update with "status", returns information
    about the current routine status
-  resume: Resumes a currently waiting routine
-  stop: Stops a routine and also removes it (so resulting in two calls to
    `getRoutineUpdate`)

The following table shows the mapping from dpsl API calls to Chrome Extension
API calls; the input parameters are the same.

| **DPSL function** | **Chrome Extension function** |
| ----------------- | ----------------------------- |
| dpsl.diagnostics.getAvailableRoutines | chrome.os.diagnostics.getAvailableRoutines |
| dpsl.diagnostics.audio.runAudioDriverRoutine | chrome.os.diagnostics.runAudioDriverRoutine |
| dpsl.diagnostics.battery.runCapacityRoutine | chrome.os.diagnostics.runBatteryCapacityRoutine |
| dpsl.diagnostics.battery.runHealthRoutine | chrome.os.diagnostics.runBatteryHealthRoutine |
| dpsl.diagnostics.battery.runDischargeRoutine | chrome.os.diagnostics.runBatteryDischargeRoutine |
| dpsl.diagnostics.battery.runChargeRoutine | chrome.os.diagnostics.runBatteryChargeRoutine |
| dpsl.diagnostics.bluetooth.runBluetoothPowerRoutine | chrome.os.diagnostics.runBluetoothPowerRoutine |
| dpsl.diagnostics.bluetooth.runBluetoothDiscoveryRoutine | chrome.os.diagnostics.runBluetoothDiscoveryRoutine |
| dpsl.diagnostics.bluetooth.runBluetoothScanningRoutine | chrome.os.diagnostics.runBluetoothScanningRoutine |
| dpsl.diagnostics.bluetooth.runBluetoothPairingRoutine | chrome.os.diagnostics.runBluetoothPairingRoutine |
| dpsl.diagnostics.cpu.runCacheRoutine | chrome.os.diagnostics.runCpuCacheRoutine |
| dpsl.diagnostics.cpu.runStressRoutine | chrome.os.diagnostics.runCpuStressRoutine |
| dpsl.diagnostics.cpu.runFloatingPointAccuracyRoutine | chrome.os.diagnostics.runCpuFloatingPointAccuracyRoutine |
| dpsl.diagnostics.cpu.runPrimeSearchRoutine | chrome.os.diagnostics.runCpuPrimeSearchRoutine |
| dpsl.diagnostics.disk.runReadRoutine | chrome.os.diagnostics.runDiskReadRoutine |
| dpsl.diagnostics.emmc.runEmmcLifetimeRoutine | chrome.os.diagnostics.runEmmcLifetimeRoutine |
| dpsl.diagnostics.hardwareButton.runPowerButtonRoutine | chrome.os.diagnostics.runPowerButtonRoutine |
| dpsl.diagnostics.memory.runMemoryRoutine | chrome.os.diagnostics.runMemoryRoutine |
| dpsl.diagnostics.nvme.runSmartctlCheckRoutine | chrome.os.diagnostics.runSmartctlCheckRoutine |
| dpsl.diagnostics.nvme.runWearLevelRoutine | chrome.os.diagnostics.runNvmeWearLevelRoutine |
| dpsl.diagnostics.nvme.runSelfTestRoutine | chrome.os.diagnostics.runNvmeSelfTestRoutine |
| dpsl.diagnostics.network.runLanConnectivityRoutine | chrome.os.diagnostics.runLanConnectivityRoutine |
| dpsl.diagnostics.network.runSignalStrengthRoutine | chrome.os.diagnostics.runSignalStrengthRoutine |
| dpsl.diagnostics.network.runDnsResolverPresentRoutine | chrome.os.diagnostics.runDnsResolverPresentRoutine |
| dpsl.diagnostics.network.runDnsResolutionRoutine | chrome.os.diagnostics.runDnsResolutionRoutine |
| dpsl.diagnostics.network.runGatewayCanBePingedRoutine | chrome.os.diagnostics.runGatewayCanBePingedRoutine |
| dpsl.diagnostics.power.runAcPowerRoutine | chrome.os.diagnostics.runAcPowerRoutine |
| dpsl.diagnostics.sensor.runSensitiveSensorRoutine | chrome.os.diagnostics.runSensitiveSensorRoutine |
| dpsl.diagnostics.sensor.runFingerprintAliveRoutine | chrome.os.diagnostics.runFingerprintAliveRoutine |
| dpsl.diagnostics.ufs.runUfsLifetimeRoutine | chrome.os.diagnostics.runUfsLifetimeRoutine |
