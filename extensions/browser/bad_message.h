// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BAD_MESSAGE_H_
#define EXTENSIONS_BROWSER_BAD_MESSAGE_H_

namespace content {
class BrowserMessageFilter;
class RenderProcessHost;
}

namespace extensions {
namespace bad_message {

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
  AVG_NULL_AVG = 6,
  // Invalid decrement of an Extensions SW ref count.
  ESWMF_INVALID_DECREMENT_ACTIVITY = 7,
  EFD_BAD_MESSAGE = 8,
  EFD_BAD_MESSAGE_WORKER = 9,
  WVG_PARTITION_ID_NOT_UTF8 = 10,
  ESWMF_BAD_EVENT_ACK = 11,
  MHVG_INVALID_PLUGIN_FRAME_ID = 12,
  EMF_INVALID_CHANNEL_SOURCE_TYPE = 13,
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

// Called when a browser message filter receives a bad IPC message from a
// renderer or other child process. Logs the event, records a histogram metric
// for the |reason|, and terminates the process for |filter|.
void ReceivedBadMessage(content::BrowserMessageFilter* filter,
                        BadMessageReason reason);

}  // namespace bad_message
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BAD_MESSAGE_H_
