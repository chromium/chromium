# Updater History Event Schema

This document describes the JSON schema for events recorded by the updater. Each

event is a record of an updater action. Events are serialized as JSON objects in
a JSONL file (one object per line).

## Required Fields

The schema indicates that some fields are `required`, to clarify this indicates
that:

1. A compliant serialization implementation will refuse messages which do not contain all required fields.
2. Deserialization process may consider a message missing one or more required fields as ill-formed, but need not reject the message when data recovery is best-effort.

## Base Event Properties

All events share a common set of base properties:

| Property        | Type    | Description                                                                                                                                                                                     | Required |
|-:---------------|-:-------|-:-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|-:--------|
| `eventType`     | string  | The type of event being recorded. See the _Event Types_ section below for possible values.                                                                                                      | Yes      |
| `eventId`       | string  | A unique identifier for the event. For events with a duration, this ID links the `START` and `END` records.                                                                                     | Yes      |
| `deviceUptime`  | string  | The uptime of the device in integer microseconds.                                                                                                                                               | Yes      |
| `pid`           | integer | The process ID of the updater instance that emitted the event.                                                                                                                                  | Yes      |
| `processToken`  | string  | A per-process random value to discriminate between processes with the same PID due to OS-reuse.                                                                                                 | Yes      |
| `bound`         | string  | Indicates if the record marks the beginning (`START`), end (`END`), or an instantaneous event (`INSTANT`). Defaults to `INSTANT`.                                                               | No       |
| `errors`        | array   | An array of error objects encountered during the event. Each object has: `category` (integer), `code` (integer), `extracode1` (integer). An empty array has the same meaning as an unset array. | No       |

## Event Types

The specific details of an event are determined by its `eventType` and `bound`.
The schema defines the following event types:

### `INSTALL`: Application Installation

* **Description:** Recorded when an application installation starts or
  completes.
* **Bounds:**
  * `START`:
    * `appId` (string, required): The ID of the application being installed.
  * `END`: No specific properties.
    * `version` (string): The version of the application that the updater tried
      to install.

### `UNINSTALL`: Application Uninstallation

* **Description:** Recorded when an application uninstallation starts or
  completes.
* **Bounds:**
  * `START`:
    * `appId` (string, required): The ID of the application being uninstalled.
    * `version` (string, required): The version of the application being
      uninstalled.
    * `reason` (string, required): Enum: `”UNINSTALLED”`, `”USER_NOT_AN_OWNER”`,
      `”NO_APPS_REMAIN”`, `”NEVER_HAD_APPS”`.
  * `END`: No specific properties.

### `QUALIFY`: Updater Qualification

* **Description:** Recorded when the updater starts or completes its
  self-qualification process to become the active version.
* **Bounds:**
  * `START`: No specific properties.
  * `END`:
    * `qualified` (boolean): Whether the updater version successfully qualified.

### `ACTIVATE`: Updater Activation

* **Description:** Recorded when the updater starts or completes its activation
  process.
* **Bounds:**
  * `START`: No specific properties.
  * `END`:
    * `activated` (boolean, required): Whether the running updater version was
      activated.

### `PERSISTED_DATA`: Persisted Data Snapshot

* **Description:** Records a snapshot of data persisted by the updater, such as
  application registrations and policy states. This is an `INSTANT` event.
* **Properties:**
  * `eulaRequired` (boolean, required): Whether a user responsible for this
    system needs to accept a EULA.
  * `lastChecked` (string): Timestamp of the last successful update check (RFC
    3339 date-time).
  * `lastStarted` (string): Timestamp of the last time the updater ran its
    periodic tasks (RFC 3339 date-time).
  * `registeredApps` (array): A list of applications registered with the
    updater. Each item is an object with:
    * `appId` (string, required): The app's unique identifier.
    * `version` (string, required): The registered version of the app.
    * `cohort` (string): The cohort assignment of the app.
    * `brandCode` (string): The brand code of the app.

### `POST_REQUEST`: HTTP Post Request

* **Description:** Recorded when an HTTP POST request is initiated or completed.
* **Bounds:**
  * `START`:
    * `request` (string, required): The base64-encoded HTTP request body.
  * `END`:
    * `response` (string): The base64-encoded HTTP response body.

### `LOAD_POLICY`: Load Policy

* **Description:** Recorded when the updater begins or finishes computing the
  effective enterprise policy set.
* **Bounds:**
  * `START`: No specific properties.
  * `END`:
    * `policySet` (object): The enterprise policy set. See the _Policy Set_
      section below.

### `UPDATE`: Application Update Check

* **Description:** Recorded when the updater starts or finishes an update
  check/process for an application.
* **Bounds:**
  * `START`:
    * `appId` (string): The ID of the application being checked.
    * `priority` (string): Enum: `"BACKGROUND"`, `"FOREGROUND"`.
  * `END`:
    * `outcome` (string): The result of the update operation. Enum: possible
      values are the upper snake case UpdaterState::State enum labels defined in
      UpdateService. Includes `"UPDATED"`, `"NO_UPDATE"`, and `"UPDATE_ERROR"`.
    * `nextVersion` (string): The version of the application which the updater
      tried to update to.

### `UPDATER_PROCESS`: Updater Process Lifetime

* **Description:** Recorded when an updater process starts or terminates.
* **Bounds:**
  * `START`:
    * `commandLine` (string): The full command line of the updater process
      invocation.
    * `timestamp` (string): The system time when the event began (RFC 3339
      date-time).
    * `updaterVersion` (string): The version of the updater.
    * `scope` (string): The scope in which the updater is running. Enum:
      `"USER"`, `"SYSTEM"`.
    * `osPlatform` (string): The operating system platform.
    * `osVersion` (string): The operating system version.
    * `osArchitecture` (string): The architecture of the operating system.
    * `updaterArchitecture` (string): The architecture of the updater binary.
    * `deviceUptime` (string): The uptime of the system in integer microseconds.
    * `parentPid` (integer): The parent process id, which allows the process
      tree of the updater to be reconstructed.
  * `END`: No specific properties.
    * `exitCode` (integer): The exit code of the process.

### `APP_COMMAND`: App Command Invocation

* **Description:** Recorded when an app command is invoked by the updater.
* **Bounds:**
  * `START`:
    * `appId` (string, required): The ID of the app owning the command.
    * `commandLine` (string): The full command line used to invoke the app
      command.
  * `END`:
    * `exitCode` (integer): The exit code of the app command.
    * `output` (string): The output generated by the app command.

## Common Complex Types

### Policy Set

* **Description:** Represents the enterprise policy set governing the updater's
  behavior.
* **Properties:**
  * `policiesByName` (object, required): An object mapping policy names to their
    details. Each value in this map is an object with:
    * `valuesBySource` (object, required): Policy values provided by different
      sources. Values are of arbitrary type. Potential keys include:
      * `default`: The built-in default value.
      * `externalConstants`: Value from external constants file (test only).
      * `platform`: Value from OS policy manager (e.g., Group Policy, Managed
        Preferences).
      * `cloud`: Value from cloud service.
    * `prevailingSource` (string, required): A indicating which key within
      `valuesBySource` contains the authoritative value used by the updater
      (e.g., `platform`).
  * `policiesByAppId` (object, required): An object mapping application
    identifiers to per-app policy sets. Each value in this map is an object with
    the same structure as `policiesByName`, as defined above.
