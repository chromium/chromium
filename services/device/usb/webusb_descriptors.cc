// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/usb/webusb_descriptors.h"

#include <limits>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "components/device_event_log/device_event_log.h"
#include "services/device/usb/usb_device_handle.h"
#include "url/gurl.h"

namespace device {

using mojom::UsbControlTransferRecipient;
using mojom::UsbControlTransferType;
using mojom::UsbTransferDirection;
using mojom::UsbTransferStatus;

namespace {

// These constants are defined by the Universal Serial Device 3.0 Specification
// Revision 1.0.
const uint8_t kGetDescriptorRequest = 0x06;

const uint8_t kBosDescriptorType = 0x0F;
const uint8_t kDeviceCapabilityDescriptorType = 0x10;

const uint8_t kPlatformDevCapabilityType = 0x05;

// These constants are defined by the WebUSB specification:
// http://wicg.github.io/webusb/
const uint8_t kGetUrlRequest = 0x02;

const uint8_t kWebUsbCapabilityUUID[16] = {
    // Little-endian encoding of {3408b638-09a9-47a0-8bfd-a0768815b665}.
    0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47,
    0x8B, 0xFD, 0xA0, 0x76, 0x88, 0x15, 0xB6, 0x65};

const size_t kMaxControlTransferLength = std::numeric_limits<uint8_t>::max();
const int kControlTransferTimeoutMs = 2000;  // 2 seconds

using ReadCompatabilityDescriptorCallback = base::OnceCallback<void(
    const std::optional<WebUsbPlatformCapabilityDescriptor>& descriptor)>;
using ReadLandingPageCallback =
    base::OnceCallback<void(const GURL& landing_page)>;

void OnReadLandingPage(uint8_t landing_page_id,
                       ReadLandingPageCallback callback,
                       UsbTransferStatus status,
                       scoped_refptr<base::RefCountedBytes> buffer,
                       size_t length) {
  if (status != UsbTransferStatus::COMPLETED) {
    USB_LOG(EVENT) << "Failed to read WebUSB URL descriptor: "
                   << static_cast<int>(landing_page_id);
    std::move(callback).Run(GURL());
    return;
  }

  GURL url;
  ParseWebUsbUrlDescriptor(base::make_span(buffer->data(), length), &url);
  std::move(callback).Run(url);
}

void OnReadBosDescriptor(scoped_refptr<UsbDeviceHandle> device_handle,
                         ReadCompatabilityDescriptorCallback callback,
                         UsbTransferStatus status,
                         scoped_refptr<base::RefCountedBytes> buffer,
                         size_t length) {
  if (status != UsbTransferStatus::COMPLETED) {
    USB_LOG(EVENT) << "Failed to read BOS descriptor.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  WebUsbPlatformCapabilityDescriptor descriptor;
  if (!descriptor.ParseFromBosDescriptor(
          base::make_span(buffer->data(), length))) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  std::move(callback).Run(descriptor);
}

void OnReadBosDescriptorHeader(scoped_refptr<UsbDeviceHandle> device_handle,
                               ReadCompatabilityDescriptorCallback callback,
                               UsbTransferStatus status,
                               scoped_refptr<base::RefCountedBytes> buffer,
                               size_t length) {
  if (status != UsbTransferStatus::COMPLETED || length != 5) {
    USB_LOG(EVENT) << "Failed to read BOS descriptor header.";
    std::move(callback).Run(std::nullopt);
    return;
  }

  const uint8_t* data = buffer->data();
  uint16_t new_length = data[2] | (data[3] << 8);
  auto new_buffer = base::MakeRefCounted<base::RefCountedBytes>(new_length);
  device_handle->ControlTransfer(
      UsbTransferDirection::INBOUND, UsbControlTransferType::STANDARD,
      UsbControlTransferRecipient::DEVICE, kGetDescriptorRequest,
      kBosDescriptorType << 8, 0, new_buffer, kControlTransferTimeoutMs,
      base::BindOnce(&OnReadBosDescriptor, device_handle, std::move(callback)));
}

void OnReadWebUsbCapabilityDescriptor(
    scoped_refptr<UsbDeviceHandle> device_handle,
    ReadLandingPageCallback callback,
    const std::optional<WebUsbPlatformCapabilityDescriptor>& descriptor) {
  if (!descriptor || !descriptor->landing_page_id) {
    std::move(callback).Run(GURL());
    return;
  }

  ReadWebUsbLandingPage(descriptor->vendor_code, descriptor->landing_page_id,
                        device_handle, std::move(callback));
}

}  // namespace

WebUsbPlatformCapabilityDescriptor::WebUsbPlatformCapabilityDescriptor()
    : version(0), vendor_code(0) {}

WebUsbPlatformCapabilityDescriptor::~WebUsbPlatformCapabilityDescriptor() =
    default;

bool WebUsbPlatformCapabilityDescriptor::ParseFromBosDescriptor(
    base::span<const uint8_t> bytes) {
  if (bytes.size() < 5) {
    // Too short for the BOS descriptor header.
    return false;
  }

  // Validate the BOS descriptor, defined in Table 9-12 of the Universal Serial
  // Bus 3.1 Specification, Revision 1.0.
  uint16_t total_length = bytes[2] + (bytes[3] << 8);
  if (bytes[0] != 5 ||                                    // bLength
      bytes[1] != kBosDescriptorType ||                   // bDescriptorType
      5 > total_length || total_length > bytes.size()) {  // wTotalLength
    return false;
  }

  uint8_t num_device_caps = bytes[4];
  auto it = bytes.begin();
  auto end = it + total_length;
  std::advance(it, 5);

  uint8_t length = 0;
  for (size_t i = 0; i < num_device_caps; ++i, std::advance(it, length)) {
    if (it == end) {
      return false;
    }

    // Validate the Device Capability descriptor, defined in Table 9-13 of the
    // Universal Serial Bus 3.1 Specification, Revision 1.0.
    length = it[0];
    if (length < 3 || std::distance(it, end) < length ||  // bLength
        it[1] != kDeviceCapabilityDescriptorType) {       // bDescriptorType
      return false;
    }

    if (it[2] != kPlatformDevCapabilityType) {  // bDevCapabilityType
      continue;
    }

    // Validate the Platform Capability Descriptor, defined in Table 9-18 of the
    // Universal Serial Bus 3.1 Specification, Revision 1.0.
    if (length < 20) {
      // Platform capability descriptors must be at least 20 bytes.
      return false;
    }

    if (memcmp(&it[4], kWebUsbCapabilityUUID, sizeof(kWebUsbCapabilityUUID)) !=
        0) {  // PlatformCapabilityUUID
      continue;
    }

    if (length < 22) {
      // The WebUSB capability descriptor must be at least 22 bytes (to allow
      // for future versions).
      return false;
    }

    version = it[20] + (it[21] << 8);  // bcdVersion
    if (version < 0x0100) {
      continue;
    }

    // Version 1.0 defines two fields for a total length of 24 bytes.
    if (length != 24) {
      return false;
    }

    vendor_code = it[22];
    landing_page_id = it[23];
    return true;
  }

  return false;
}

// Parses a WebUSB URL Descriptor:
// https://wicg.github.io/webusb/#url-descriptor
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |     length    |     type      |    prefix     |    data[0]    |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |     data[1]   |      ...
// +-+-+-+-+-+-+-+-+-+-+-+------
bool ParseWebUsbUrlDescriptor(base::span<const uint8_t> bytes, GURL* output) {
  const uint8_t kDescriptorType = 0x03;
  const uint8_t kDescriptorMinLength = 3;

  if (bytes.size() < kDescriptorMinLength) {
    return false;
  }

  // Validate that the length is consistent and fits within the buffer.
  uint8_t length = bytes[0];
  if (length < kDescriptorMinLength || length > bytes.size() ||
      bytes[1] != kDescriptorType) {
    return false;
  }

  // Look up the URL prefix and append the rest of the data in the descriptor.
  std::string url;
  switch (bytes[2]) {
    case 0:
      url.append("http://");
      break;
    case 1:
      url.append("https://");
      break;
    case 255:  // 255 indicates that the entire URL is encoded in the URL field.
      break;
    default:
      return false;
  }
  url.append(reinterpret_cast<const char*>(bytes.data() + 3), length - 3);

  *output = GURL(url);
  if (!output->is_valid()) {
    return false;
  }

  return true;
}

void ReadWebUsbLandingPage(uint8_t vendor_code,
                           uint8_t landing_page_id,
                           scoped_refptr<UsbDeviceHandle> device_handle,
                           ReadLandingPageCallback callback) {
  auto buffer =
      base::MakeRefCounted<base::RefCountedBytes>(kMaxControlTransferLength);
  device_handle->ControlTransfer(
      UsbTransferDirection::INBOUND, UsbControlTransferType::VENDOR,
      UsbControlTransferRecipient::DEVICE, vendor_code, landing_page_id,
      kGetUrlRequest, buffer, kControlTransferTimeoutMs,
      base::BindOnce(&OnReadLandingPage, landing_page_id, std::move(callback)));
}

void ReadWebUsbCapabilityDescriptor(
    scoped_refptr<UsbDeviceHandle> device_handle,
    ReadCompatabilityDescriptorCallback callback) {
  auto buffer = base::MakeRefCounted<base::RefCountedBytes>(5);
  device_handle->ControlTransfer(
      UsbTransferDirection::INBOUND, UsbControlTransferType::STANDARD,
      UsbControlTransferRecipient::DEVICE, kGetDescriptorRequest,
      kBosDescriptorType << 8, 0, buffer, kControlTransferTimeoutMs,
      base::BindOnce(&OnReadBosDescriptorHeader, device_handle,
                     std::move(callback)));
}

void ReadWebUsbDescriptors(scoped_refptr<UsbDeviceHandle> device_handle,
                           ReadLandingPageCallback callback) {
  ReadWebUsbCapabilityDescriptor(
      device_handle, base::BindOnce(&OnReadWebUsbCapabilityDescriptor,
                                    device_handle, std::move(callback)));
}

}  // namespace device
