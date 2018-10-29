#include "net/third_party/quic/core/frames/quic_retire_connection_id_frame.h"
#include "net/third_party/quic/core/quic_constants.h"

namespace quic {

QuicRetireConnectionIdFrame::QuicRetireConnectionIdFrame()
    : control_frame_id(kInvalidControlFrameId), sequence_number(0) {}

QuicRetireConnectionIdFrame::QuicRetireConnectionIdFrame(
    QuicControlFrameId control_frame_id,
    QuicConnectionIdSequenceNumber sequence_number)
    : control_frame_id(control_frame_id), sequence_number(sequence_number) {}

std::ostream& operator<<(std::ostream& os,
                         const QuicRetireConnectionIdFrame& frame) {
  os << "{ control_frame_id: " << frame.control_frame_id
     << ", sequence_number: " << frame.sequence_number << " }\n";
  return os;
}

}  // namespace quic
