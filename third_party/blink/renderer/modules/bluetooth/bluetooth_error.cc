// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/bluetooth/bluetooth_error.h"

#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

const char kGATTServerNotConnectedBase[] =
    "GATT Server is disconnected. "
    "Cannot %s. (Re)connect first with `device.gatt.connect`.";

}  // namespace

// static
String BluetoothError::CreateNotConnectedExceptionMessage(
    BluetoothOperation operation) {
  const char* operation_string = nullptr;
  switch (operation) {
    case BluetoothOperation::kServicesRetrieval:
      operation_string = "retrieve services";
      break;
    case BluetoothOperation::kCharacteristicsRetrieval:
      operation_string = "retrieve characteristics";
      break;
    case BluetoothOperation::kDescriptorsRetrieval:
      operation_string = "retrieve descriptors";
      break;
    case BluetoothOperation::kGATT:
      operation_string = "perform GATT operations";
      break;
  }
  return String::Format(kGATTServerNotConnectedBase, operation_string);
}

// static
DOMException* BluetoothError::CreateNotConnectedException(
    BluetoothOperation operation) {
  return MakeGarbageCollected<DOMException>(
      DOMExceptionCode::kNetworkError,
      CreateNotConnectedExceptionMessage(operation));
}

// static
DOMException* BluetoothError::CreateDOMException(
    BluetoothErrorCode error,
    const String& detailed_message) {
  switch (error) {
    case BluetoothErrorCode::kInvalidService:
    case BluetoothErrorCode::kInvalidCharacteristic:
    case BluetoothErrorCode::kInvalidDescriptor:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kInvalidStateError, detailed_message);
    case BluetoothErrorCode::kServiceNotFound:
    case BluetoothErrorCode::kCharacteristicNotFound:
    case BluetoothErrorCode::kDescriptorNotFound:
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kNotFoundError, detailed_message);
  }
  NOTREACHED_IN_MIGRATION();
  return MakeGarbageCollected<DOMException>(DOMExceptionCode::kUnknownError);
}

// static
DOMException* BluetoothError::CreateDOMException(
    mojom::blink::WebBluetoothResult error) {
  switch (error) {
    case mojom::blink::WebBluetoothResult::SUCCESS:
    case mojom::blink::WebBluetoothResult::SERVICE_NOT_FOUND:
    case mojom::blink::WebBluetoothResult::CHARACTERISTIC_NOT_FOUND:
    case mojom::blink::WebBluetoothResult::DESCRIPTOR_NOT_FOUND:
      // The above result codes are not expected here. SUCCESS is not
      // an error and the others have a detailed message and are
      // expected to be redirected to the switch above that handles
      // BluetoothErrorCode.
      NOTREACHED_IN_MIGRATION();
      return MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kUnknownError);
#define MAP_ERROR(enumeration, name, message)         \
  case mojom::blink::WebBluetoothResult::enumeration: \
    return MakeGarbageCollected<DOMException>(name, message);

      // AbortErrors:
      MAP_ERROR(WATCH_ADVERTISEMENTS_ABORTED, DOMExceptionCode::kAbortError,
                "The Bluetooth operation was cancelled.");

      // InvalidModificationErrors:
      MAP_ERROR(GATT_INVALID_ATTRIBUTE_LENGTH,
                DOMExceptionCode::kInvalidModificationError,
                "GATT Error: invalid attribute length.");
      MAP_ERROR(CONNECT_INVALID_ARGS,
                DOMExceptionCode::kInvalidModificationError,
                "Connection Error: invalid arguments.");

      // InvalidStateErrors:
      MAP_ERROR(SERVICE_NO_LONGER_EXISTS, DOMExceptionCode::kInvalidStateError,
                "GATT Service no longer exists.");
      MAP_ERROR(CHARACTERISTIC_NO_LONGER_EXISTS,
                DOMExceptionCode::kInvalidStateError,
                "GATT Characteristic no longer exists.");
      MAP_ERROR(DESCRIPTOR_NO_LONGER_EXISTS,
                DOMExceptionCode::kInvalidStateError,
                "GATT Descriptor no longer exists.");
      MAP_ERROR(PROMPT_CANCELED, DOMExceptionCode::kInvalidStateError,
                "User canceled the permission prompt.");
      MAP_ERROR(CONNECT_NOT_READY, DOMExceptionCode::kInvalidStateError,
                "Connection Error: Not ready.");
      MAP_ERROR(CONNECT_ALREADY_CONNECTED, DOMExceptionCode::kInvalidStateError,
                "Connection Error: Already connected.");
      MAP_ERROR(CONNECT_ALREADY_EXISTS, DOMExceptionCode::kInvalidStateError,
                "Connection Error: Already exists.");
      MAP_ERROR(CONNECT_NOT_CONNECTED, DOMExceptionCode::kInvalidStateError,
                "Connection Error: Not connected.");
      MAP_ERROR(CONNECT_NON_AUTH_TIMEOUT, DOMExceptionCode::kInvalidStateError,
                "Connection Error: Non-authentication timeout.");

      // NetworkErrors:
      MAP_ERROR(CONNECT_ALREADY_IN_PROGRESS, DOMExceptionCode::kNetworkError,
                "Connection already in progress.");
      MAP_ERROR(CONNECT_AUTH_CANCELED, DOMExceptionCode::kNetworkError,
                "Authentication canceled.");
      MAP_ERROR(CONNECT_AUTH_FAILED, DOMExceptionCode::kNetworkError,
                "Authentication failed.");
      MAP_ERROR(CONNECT_AUTH_REJECTED, DOMExceptionCode::kNetworkError,
                "Authentication rejected.");
      MAP_ERROR(CONNECT_AUTH_TIMEOUT, DOMExceptionCode::kNetworkError,
                "Authentication timeout.");
      MAP_ERROR(CONNECT_UNKNOWN_ERROR, DOMExceptionCode::kNetworkError,
                "Unknown error when connecting to the device.");
      MAP_ERROR(CONNECT_UNKNOWN_FAILURE, DOMExceptionCode::kNetworkError,
                "Connection failed for unknown reason.");
      MAP_ERROR(CONNECT_UNSUPPORTED_DEVICE, DOMExceptionCode::kNetworkError,
                "Unsupported device.");
      MAP_ERROR(DEVICE_NO_LONGER_IN_RANGE, DOMExceptionCode::kNetworkError,
                "Bluetooth Device is no longer in range.");
      MAP_ERROR(GATT_NOT_PAIRED, DOMExceptionCode::kNetworkError,
                "GATT Error: Not paired.");
      MAP_ERROR(GATT_OPERATION_IN_PROGRESS, DOMExceptionCode::kNetworkError,
                "GATT operation already in progress.");
      MAP_ERROR(CONNECT_CONN_FAILED, DOMExceptionCode::kNetworkError,
                "Connection Error: Connection attempt failed.");

      // NotFoundErrors:
      MAP_ERROR(WEB_BLUETOOTH_NOT_SUPPORTED, DOMExceptionCode::kNotFoundError,
                "Web Bluetooth is not supported on this platform. For a list "
                "of supported platforms see: https://goo.gl/J6ASzs");
      MAP_ERROR(NO_BLUETOOTH_ADAPTER, DOMExceptionCode::kNotFoundError,
                "Bluetooth adapter not available.");
      MAP_ERROR(CHOSEN_DEVICE_VANISHED, DOMExceptionCode::kNotFoundError,
                "User selected a device that doesn't exist anymore.");
      MAP_ERROR(CHOOSER_CANCELLED, DOMExceptionCode::kNotFoundError,
                "User cancelled the requestDevice() chooser.");
      MAP_ERROR(CHOOSER_NOT_SHOWN_API_GLOBALLY_DISABLED,
                DOMExceptionCode::kNotFoundError,
                "Web Bluetooth API globally disabled.");
      MAP_ERROR(CHOOSER_NOT_SHOWN_API_LOCALLY_DISABLED,
                DOMExceptionCode::kNotFoundError,
                "User or their enterprise policy has disabled Web Bluetooth.");
      MAP_ERROR(
          CHOOSER_NOT_SHOWN_USER_DENIED_PERMISSION_TO_SCAN,
          DOMExceptionCode::kNotFoundError,
          "User denied the browser permission to scan for Bluetooth devices.");
      MAP_ERROR(NO_SERVICES_FOUND, DOMExceptionCode::kNotFoundError,
                "No Services found in device.");
      MAP_ERROR(NO_CHARACTERISTICS_FOUND, DOMExceptionCode::kNotFoundError,
                "No Characteristics found in service.");
      MAP_ERROR(NO_DESCRIPTORS_FOUND, DOMExceptionCode::kNotFoundError,
                "No Descriptors found in Characteristic.");
      MAP_ERROR(BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE,
                DOMExceptionCode::kNotFoundError,
                "Bluetooth Low Energy not available.");
      MAP_ERROR(CONNECT_DOES_NOT_EXIST, DOMExceptionCode::kNotFoundError,
                "Does not exist.");

      // NotSupportedErrors:
      MAP_ERROR(GATT_UNKNOWN_ERROR, DOMExceptionCode::kNotSupportedError,
                "GATT Error Unknown.");
      MAP_ERROR(GATT_UNKNOWN_FAILURE, DOMExceptionCode::kNotSupportedError,
                "GATT operation failed for unknown reason.");
      MAP_ERROR(GATT_NOT_PERMITTED, DOMExceptionCode::kNotSupportedError,
                "GATT operation not permitted.");
      MAP_ERROR(GATT_NOT_SUPPORTED, DOMExceptionCode::kNotSupportedError,
                "GATT Error: Not supported.");
      MAP_ERROR(GATT_UNTRANSLATED_ERROR_CODE,
                DOMExceptionCode::kNotSupportedError,
                "GATT Error: Unknown GattErrorCode.");

      // SecurityErrors:
      MAP_ERROR(GATT_NOT_AUTHORIZED, DOMExceptionCode::kSecurityError,
                "GATT operation not authorized.");
      MAP_ERROR(BLOCKLISTED_CHARACTERISTIC_UUID,
                DOMExceptionCode::kSecurityError,
                "getCharacteristic(s) called with blocklisted UUID. "
                "https://goo.gl/4NeimX");
      MAP_ERROR(BLOCKLISTED_DESCRIPTOR_UUID, DOMExceptionCode::kSecurityError,
                "getDescriptor(s) called with blocklisted UUID. "
                "https://goo.gl/4NeimX");
      MAP_ERROR(BLOCKLISTED_READ, DOMExceptionCode::kSecurityError,
                "readValue() called on blocklisted object marked "
                "exclude-reads. https://goo.gl/4NeimX");
      MAP_ERROR(BLOCKLISTED_WRITE, DOMExceptionCode::kSecurityError,
                "writeValue() called on blocklisted object marked "
                "exclude-writes. https://goo.gl/4NeimX");
      MAP_ERROR(NOT_ALLOWED_TO_ACCESS_ANY_SERVICE,
                DOMExceptionCode::kSecurityError,
                "Origin is not allowed to access any service. Tip: Add the "
                "service UUID to 'optionalServices' in requestDevice() "
                "options. https://goo.gl/HxfxSQ");
      MAP_ERROR(NOT_ALLOWED_TO_ACCESS_SERVICE, DOMExceptionCode::kSecurityError,
                "Origin is not allowed to access the service. Tip: Add the "
                "service UUID to 'optionalServices' in requestDevice() "
                "options. https://goo.gl/HxfxSQ");
      MAP_ERROR(REQUEST_DEVICE_WITH_BLOCKLISTED_UUID_OR_MANUFACTURER_DATA,
                DOMExceptionCode::kSecurityError,
                "requestDevice() called with a filter containing a blocklisted "
                "UUID or manufacturer data. https://goo.gl/4NeimX");
      MAP_ERROR(PERMISSIONS_POLICY_VIOLATION, DOMExceptionCode::kSecurityError,
                "Access to the feature \"bluetooth\" is disallowed by "
                "permissions policy.");

      // NotAllowedErrors:
      MAP_ERROR(SCANNING_BLOCKED, DOMExceptionCode::kNotAllowedError,
                "requestLEScan() call is blocked by user.");

      // UnknownErrors:
      MAP_ERROR(CONNECT_NO_MEMORY, DOMExceptionCode::kUnknownError,
                "Connection Error: An internal error has occurred.");
      MAP_ERROR(CONNECT_JNI_ENVIRONMENT, DOMExceptionCode::kUnknownError,
                "Connection Error: An internal error has occurred.");
      MAP_ERROR(CONNECT_JNI_THREAD_ATTACH, DOMExceptionCode::kUnknownError,
                "Connection Error: An internal error has occurred.");
      MAP_ERROR(CONNECT_WAKELOCK, DOMExceptionCode::kUnknownError,
                "Connection Error: An internal error has occurred.");
      MAP_ERROR(CONNECT_UNEXPECTED_STATE, DOMExceptionCode::kUnknownError,
                "Connection Error: An internal error has occurred.");
      MAP_ERROR(CONNECT_SOCKET_ERROR, DOMExceptionCode::kUnknownError,
                "Connection Error: An internal error has occurred.");

#undef MAP_ERROR
  }

  NOTREACHED_IN_MIGRATION();
  return MakeGarbageCollected<DOMException>(DOMExceptionCode::kUnknownError);
}

}  // namespace blink
