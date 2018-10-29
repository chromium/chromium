#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_RETIRE_CONNECTION_ID_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_RETIRE_CONNECTION_ID_FRAME_H_

#include <ostream>

#include "net/third_party/quic/core/quic_error_codes.h"
#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_uint128.h"

namespace quic {

struct QUIC_EXPORT_PRIVATE QuicRetireConnectionIdFrame {
  QuicRetireConnectionIdFrame();
  QuicRetireConnectionIdFrame(QuicControlFrameId control_frame_id,
                              QuicConnectionIdSequenceNumber sequence_number);

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicRetireConnectionIdFrame& frame);

  // A unique identifier of this control frame. 0 when this frame is received,
  // and non-zero when sent.
  QuicControlFrameId control_frame_id;
  QuicConnectionIdSequenceNumber sequence_number;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_RETIRE_CONNECTION_ID_FRAME_H_
