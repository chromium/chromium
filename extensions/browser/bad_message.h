// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BAD_MESSAGE_H_
#define EXTENSIONS_BROWSER_BAD_MESSAGE_H_

namespace content {
class RenderProcessHost;
}

// Comparison of `extensions::bad_message::ReceivedBadMessage` vs
// `mojo::ReportBadMessage`:
//
// * Both are an acceptable way to terminate a renderer process that has
//   sent a malformed IPC.
// * `extensions::bad_message::ReceivedBadMessage` has the following advantages:
//     * Simplicity
//     * Granular UMA (which may help with gathering go/chrometto traces when
//       investigating unexpected reports of malformed IPCs)
// * `mojo::ReportBadMessage` has the following advantages:
//     * Can be used without knowing the `RenderProcessHost` or
//       `render_process_id` (`mojo::RenderProcessHost` can be called at any
//       time when synchronously handling a mojo method call; asynchronous bad
//       message report is possible via `mojo::GetBadMessageCallback`).
//     * It is less tightly coupled with the //extensions layer (i.e. moving
//       the code to another layer or component is easier with
//       `mojo::ReportBadMessage`).
namespace extensions::bad_message {

// The browser process often chooses to terminate a renderer if it receives
// a bad IPC message. The reasons are tracked for metrics.
//
// See also content/browser/bad_message.h.
//
// NOTE: Do not remove or reorder elements in this list. Add new entries at the
// end. Items may be renamed but do not change the values. We rely on the enum
// values in histograms.
enum BadMessageReason {
  EOG_BAD_ORIGIN = 0,
  EVG_BAD_ORIGIN = 1,
  BH_BLOB_NOT_OWNED = 2,
  EH_BAD_EVENT_ID = 3,
  AVG_BAD_INST_ID = 4,
  AVG_BAD_EXT_ID = 5,
  OBSOLETE_AVG_NULL_AVG = 6,
  // DEPRECATED_ESWMF_INVALID_DECREMENT_ACTIVITY = 7,
  EFD_BAD_MESSAGE = 8,
  EFD_BAD_MESSAGE_WORKER = 9,
  WVG_PARTITION_ID_NOT_UTF8 = 10,
  ESWMF_BAD_EVENT_ACK = 11,
  MHVG_INVALID_PLUGIN_FRAME_ID = 12,
  EMF_INVALID_CHANNEL_SOURCE_TYPE = 13,
  EMF_NO_EXTENSION_ID_FOR_EXTENSION_SOURCE = 14,
  EMF_INVALID_EXTENSION_ID_FOR_EXTENSION_SOURCE = 15,
  EMF_INVALID_EXTENSION_ID_FOR_CONTENT_SCRIPT = 16,
  EMF_INVALID_EXTENSION_ID_FOR_WORKER_CONTEXT = 17,
  EMF_INVALID_PORT_CONTEXT = 18,
  AWCI_INVALID_CALL_FROM_NOT_PRIMARY_MAIN_FRAME = 19,
  EFD_INVALID_EXTENSION_ID_FOR_PROCESS = 20,
  // DEPRECATED_EMF_INVALID_EXTENSION_ID_FOR_TAB_MSG = 21,
  EMF_NON_EXTENSION_SENDER_FRAME = 22,
  EMF_NON_EXTENSION_SENDER_NATIVE_HOST = 23,
  EMF_INVALID_SOURCE_URL = 24,
  // DEPRECATED_EMF_INVALID_SOURCE_URL_FROM_WORKER = 25,
  EMF_INVALID_OPEN_CHANNEL_TO_EXTENSION_FROM_NATIVE_HOST = 26,
  EMF_INVALID_EXTENSION_ID_FOR_WEB_PAGE = 27,
  EMF_INVALID_EXTENSION_ID_FOR_USER_SCRIPT = 28,
  EMF_INVALID_EXTERNAL_EXTENSION_ID_FOR_USER_SCRIPT = 29,
  EMF_INVALID_OPEN_CHANNEL_TO_NATIVE_APP_FROM_NATIVE_HOST = 30,
  EFH_NO_BACKGROUND_HOST_FOR_FRAME = 31,
  LEGACY_IPC_MISMATCH = 32,
  ER_SW_INVALID_LAZY_BACKGROUND_PARAM = 33,
  // Please add new elements here. The naming convention is abbreviated class
  // name (e.g. ExtensionHost becomes EH) plus a unique description of the
  // reason. After making changes, you MUST update histograms.xml by running:
  // "python tools/metrics/histograms/update_bad_message_reasons.py"
  BAD_MESSAGE_MAX
};

// Called when the browser receives a bad IPC message from a normal or an
// extension renderer. Logs the event, records a histogram metric for the
// |reason|, and terminates the process for |host|/|render_process_id|.
void ReceivedBadMessage(content::RenderProcessHost* host,
                        BadMessageReason reason);

// Same as ReceivedBadMessage above, but takes a render process id. Non-existent
// render process ids are ignored.
void ReceivedBadMessage(int render_process_id, BadMessageReason reason);

}  // namespace extensions::bad_message

#endif  // EXTENSIONS_BROWSER_BAD_MESSAGE_H_
