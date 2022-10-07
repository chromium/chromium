// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_edid_scan_win.h"

#include <objbase.h>
#include <oleauto.h>
#include <string.h>

#include "base/logging.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_variant.h"
#include "base/win/wmi.h"

namespace media {

// Short Audio Descriptor values defined in ANSI/CEA-861
enum {
  kEdidAudioLpcm = 1,
  kEdidAudioDts = 7,
  kEdidAudioDtsHd = 11,
};

namespace {

uint32_t EdidParseBlocks(const uint8_t* data, int data_size) {
  const uint8_t *block = data, *block_end = data + data_size;
  static constexpr uint8_t kBaseHeader[] = {0,    0xFF, 0xFF, 0xFF,
                                            0xFF, 0xFF, 0xFF, 0};
  constexpr uint8_t kEdidAudio = 1;
  constexpr uint8_t kExtensionTagCea = 2;
  constexpr int kBlockSize = 128;
  constexpr uint8_t kSadSize = 3;
  uint32_t bitstream_mask = 0;

  // http://crbug.com/1371473
  // TODO(dcheng): Use base::BufferIterator to parse EDID block
  // Skip base EDID structure if present
  if (block + kBlockSize <= block_end &&
      (memcmp(block, kBaseHeader, sizeof(kBaseHeader)) == 0)) {
    block += kBlockSize;
  }

  // Process CEA EDID (tag 2) extension blocks
  for (; block + kBlockSize <= block_end; block += kBlockSize) {
    if (block[0] != kExtensionTagCea)
      continue;

    // Process the audio data blocks containing Short Audio Descriptors (SADs),
    // which are three bytes each.  SADs start at byte 4 and end before the
    // byte specified in block[2].
    const uint8_t *db = block + 4, *db_end = block + block[2];
    if (db_end > block_end)
      continue;
    for (; db + kSadSize <= db_end && db[0]; db += (db[0] & 0x1F) + 1) {
      if ((db[0] >> 5) != kEdidAudio)
        continue;

      const int len = 1 + (db[0] & 0x1F);
      if (db + len > db_end)
        continue;

      for (int i = 1; i < len; i += 3) {
        switch ((db[i] >> 3) & 15) {
          case kEdidAudioLpcm:
            bitstream_mask |= kAudioBitstreamPcmLinear;
            break;
          case kEdidAudioDts:
            bitstream_mask |= kAudioBitstreamDts;
            break;
          case kEdidAudioDtsHd:
            bitstream_mask |= kAudioBitstreamDts;
            bitstream_mask |= kAudioBitstreamDtsHd;
            break;
        }
      }
    }
  }

  DVLOG(1) << "SERVICE: EdidParseBlocks bitstream mask " << bitstream_mask;
  return bitstream_mask;
}

}  // namespace

uint32_t ScanEdidBitstreams() {
  // The WMI service allows the querying of monitor-type devices which report
  // Extended Display Identification Data (EDID).  The WMI service can be
  // queried for a list of COM objects which represent the "paths" which
  // are associated with individual EDID devices.  Querying each of those
  // paths using the WmiGetMonitorRawEEdidV1Block method returns the EDID
  // blocks for those devices.  We query the extended blocks which contain
  // the Short Audio Descriptor (SAD), and parse them to obtain a bitmask
  // indicating which audio content is supported.  The mask consists of
  // AudioParameters::Format flags.  If multiple EDID devices are present,
  // the intersection of flags is reported.
  Microsoft::WRL::ComPtr<IWbemServices> wmi_services =
      base::win::CreateWmiConnection(true, L"ROOT\\WMI");
  if (!wmi_services)
    return 0;

  Microsoft::WRL::ComPtr<IWbemClassObject> get_edid_block;
  if (!base::win::CreateWmiClassMethodObject(
          wmi_services.Get(), L"WmiMonitorDescriptorMethods",
          L"WmiGetMonitorRawEEdidV1Block", &get_edid_block)) {
    return 0;
  }

  Microsoft::WRL::ComPtr<IEnumWbemClassObject> wmi_enumerator;
  HRESULT hr = wmi_services->CreateInstanceEnum(
      base::win::ScopedBstr(L"WmiMonitorDescriptorMethods").Get(),
      WBEM_FLAG_FORWARD_ONLY, nullptr, &wmi_enumerator);
  if (FAILED(hr))
    return 0;

  base::win::ScopedVariant block_id(1);
  bool first = true;
  uint32_t bitstream_mask = 0;

  while (true) {
    Microsoft::WRL::ComPtr<IWbemClassObject> class_object;
    ULONG items_returned = 0;
    hr = wmi_enumerator->Next(WBEM_INFINITE, 1, &class_object, &items_returned);
    if (FAILED(hr) || hr == WBEM_S_FALSE || items_returned == 0)
      break;

    base::win::ScopedVariant path;
    class_object->Get(L"__PATH", 0, path.Receive(), nullptr, nullptr);

    if (FAILED(get_edid_block->Put(L"BlockId", 0, block_id.AsInput(), 0)))
      break;

    Microsoft::WRL::ComPtr<IWbemClassObject> out_params;
    hr = wmi_services->ExecMethod(
        V_BSTR(path.ptr()),
        base::win::ScopedBstr(L"WmiGetMonitorRawEEdidV1Block").Get(), 0,
        nullptr, get_edid_block.Get(), &out_params, nullptr);
    if (FAILED(hr))
      break;

    base::win::ScopedVariant block_type;
    if (FAILED(out_params->Get(L"BlockType", 0, block_type.Receive(), nullptr,
                               0))) {
      continue;
    }

    base::win::ScopedVariant block_content;
    if (FAILED(out_params->Get(L"BlockContent", 0, block_content.Receive(),
                               nullptr, 0))) {
      continue;
    }

    if (V_I4(block_type.ptr()) != 255)
      continue;

    if (block_content.type() != (VT_ARRAY | VT_UI1))
      continue;

    SAFEARRAY* array = V_ARRAY(block_content.ptr());
    if (SafeArrayGetDim(array) != 1)
      continue;

    long lower_bound = 0;
    long upper_bound = 0;
    SafeArrayGetLBound(array, 1, &lower_bound);
    SafeArrayGetUBound(array, 1, &upper_bound);
    if (lower_bound || upper_bound <= lower_bound)
      continue;

    uint8_t* block = nullptr;
    SafeArrayAccessData(array, reinterpret_cast<void**>(&block));
    if (first) {
      first = false;
      bitstream_mask = EdidParseBlocks(block, upper_bound + 1);
    } else {
      bitstream_mask &= EdidParseBlocks(block, upper_bound + 1);
    }
    SafeArrayUnaccessData(array);
  }

  return bitstream_mask;
}

}  // namespace media
