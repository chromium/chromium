// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_SPDY_CORE_SPDY_DEFRAMER_VISITOR_H_
#define NET_THIRD_PARTY_SPDY_CORE_SPDY_DEFRAMER_VISITOR_H_

// Supports testing by converting callbacks to SpdyFramerVisitorInterface into
// callbacks to SpdyDeframerVisitorInterface, whose arguments are generally
// SpdyFrameIR instances. This enables a test client or test backend to operate
// at a level between the low-level callbacks of SpdyFramerVisitorInterface and
// the much higher level of entire messages (i.e. headers, body, trailers).
// Where possible the converter (SpdyTestDeframer) tries to preserve information
// that might be useful to tests (e.g. the order of headers or the amount of
// padding); the design also aims to allow tests to be concise, ideally
// supporting gMock style EXPECT_CALL(visitor, OnHeaders(...matchers...))
// without too much boilerplate.
//
// Only supports HTTP/2 for the moment.
//
// Example of usage:
//
//    SpdyFramer framer(HTTP2);
//
//    // Need to call SpdyTestDeframer::AtFrameEnd() after processing each
//    // frame, so tell SpdyFramer to stop after each.
//    framer.set_process_single_input_frame(true);
//
//    // Need the new OnHeader callbacks.
//    framer.set_use_new_methods_for_test(true);
//
//    // Create your visitor, a subclass of SpdyDeframerVisitorInterface.
//    // For example, using DeframerCallbackCollector to collect frames:
//    std::vector<CollectedFrame> collected_frames;
//    auto your_visitor = SpdyMakeUnique<DeframerCallbackCollector>(
//        &collected_frames);
//
//    // Transfer ownership of your visitor to the converter, which ensures that
//    // your visitor stays alive while the converter needs to call it.
//    auto the_deframer = SpdyTestDeframer::CreateConverter(
//       std::move(your_visitor));
//
//    // Tell the framer to notify SpdyTestDeframer of the decoded frame
//    // details.
//    framer.set_visitor(the_deframer.get());
//
//    // Process frames.
//    SpdyStringPiece input = ...
//    while (!input.empty() && !framer.HasError()) {
//      size_t consumed = framer.ProcessInput(input.data(), input.size());
//      input.remove_prefix(consumed);
//      if (framer.state() == SpdyFramer::SPDY_READY_FOR_FRAME) {
//        the_deframer->AtFrameEnd();
//      }
//    }
//
//    // Make sure that the correct frames were received. For example:
//    ASSERT_EQ(collected_frames.size(), 3);
//
//    SpdyDataIR expected1(7 /*stream_id*/, "Data Payload");
//    expected1.set_padding_len(17);
//    EXPECT_TRUE(collected_frames[0].VerifyEquals(expected1));
//
//    // Repeat for the other frames.
//
// Note that you could also seed the subclass of SpdyDeframerVisitorInterface
// with the expected frames, which it would pop-off the list as its expectations
// are met.

#include <cstdint>

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "net/third_party/spdy/core/http2_frame_decoder_adapter.h"
#include "net/third_party/spdy/core/spdy_protocol.h"
#include "net/third_party/spdy/core/spdy_protocol_test_utils.h"
#include "net/third_party/spdy/core/spdy_test_utils.h"
#include "net/third_party/spdy/platform/api/spdy_string.h"

namespace spdy {
namespace test {

// Non-lossy representation of a SETTINGS frame payload.
typedef std::vector<std::pair<SpdyKnownSettingsId, uint32_t>> SettingVector;

// StringPairVector is used to record information lost by SpdyHeaderBlock, in
// particular the order of each header entry, though it doesn't expose the
// inner details of the HPACK block, such as the type of encoding selected
// for each header entry, nor dynamic table size changes.
typedef std::pair<SpdyString, SpdyString> StringPair;
typedef std::vector<StringPair> StringPairVector;

// Forward decl.
class SpdyTestDeframer;

// Note that this only roughly captures the frames, as padding bytes are lost,
// continuation frames are combined with their leading HEADERS or PUSH_PROMISE,
// the details of the HPACK encoding are lost, leaving
// only the list of header entries (name and value strings). If really helpful,
// we could add a SpdyRawDeframerVisitorInterface that gets the HPACK bytes,
// and receives continuation frames. For more info we'd need to improve
// SpdyFramerVisitorInterface.
class SpdyDeframerVisitorInterface {
 public:
  virtual ~SpdyDeframerVisitorInterface() {}

  // Wrap a visitor in another SpdyDeframerVisitorInterface that will
  // DVLOG each call, and will then forward the calls to the wrapped visitor
  // (if provided; nullptr is OK). Takes ownership of the wrapped visitor.
  static std::unique_ptr<SpdyDeframerVisitorInterface> LogBeforeVisiting(
      std::unique_ptr<SpdyDeframerVisitorInterface> wrapped_visitor);

  virtual void OnAltSvc(std::unique_ptr<SpdyAltSvcIR> frame) {}
  virtual void OnData(std::unique_ptr<SpdyDataIR> frame) {}
  virtual void OnGoAway(std::unique_ptr<SpdyGoAwayIR> frame) {}

  // SpdyHeadersIR and SpdyPushPromiseIR each has a SpdyHeaderBlock which
  // significantly modifies the headers, so the actual header entries (name
  // and value strings) are provided in a vector.
  virtual void OnHeaders(std::unique_ptr<SpdyHeadersIR> frame,
                         std::unique_ptr<StringPairVector> headers) {}

  virtual void OnPing(std::unique_ptr<SpdyPingIR> frame) {}
  virtual void OnPingAck(std::unique_ptr<SpdyPingIR> frame);
  virtual void OnPriority(std::unique_ptr<SpdyPriorityIR> frame) {}

  // SpdyHeadersIR and SpdyPushPromiseIR each has a SpdyHeaderBlock which
  // significantly modifies the headers, so the actual header entries (name
  // and value strings) are provided in a vector.
  virtual void OnPushPromise(std::unique_ptr<SpdyPushPromiseIR> frame,
                             std::unique_ptr<StringPairVector> headers) {}

  virtual void OnRstStream(std::unique_ptr<SpdyRstStreamIR> frame) {}

  // SpdySettingsIR has a map for settings, so loses info about the order of
  // settings, and whether the same setting appeared more than once, so the
  // the actual settings (parameter and value) are provided in a vector.
  virtual void OnSettings(std::unique_ptr<SpdySettingsIR> frame,
                          std::unique_ptr<SettingVector> settings) {}

  // A settings frame with an ACK has no content, but for uniformity passing
  // a frame with the ACK flag set.
  virtual void OnSettingsAck(std::unique_ptr<SpdySettingsIR> frame);

  virtual void OnWindowUpdate(std::unique_ptr<SpdyWindowUpdateIR> frame) {}

  // The SpdyFramer will not process any more data at this point.
  virtual void OnError(http2::Http2DecoderAdapter::SpdyFramerError error,
                       SpdyTestDeframer* deframer) {}
};

class SpdyTestDeframer : public SpdyFramerVisitorInterface {
 public:
  ~SpdyTestDeframer() override {}

  // Creates a SpdyFramerVisitorInterface that builds SpdyFrameIR concrete
  // instances based on the callbacks it receives; when an entire frame is
  // decoded/reconstructed it calls the passed in SpdyDeframerVisitorInterface.
  // Transfers ownership of visitor to the new SpdyTestDeframer, which ensures
  // that it continues to exist while the SpdyTestDeframer exists.
  static std::unique_ptr<SpdyTestDeframer> CreateConverter(
      std::unique_ptr<SpdyDeframerVisitorInterface> visitor);

  // Call to notify the deframer that the SpdyFramer has returned after reaching
  // the end of decoding a frame. This is used to flush info about some frame
  // types where we don't get a clear end signal; others are flushed (i.e. the
  // appropriate call to the SpdyDeframerVisitorInterface method is invoked)
  // as they're decoded by SpdyFramer and it calls the deframer. See the
  // example in the comments at the top of this file.
  virtual bool AtFrameEnd() = 0;

 protected:
  SpdyTestDeframer() {}
  SpdyTestDeframer(const SpdyTestDeframer&) = delete;
  SpdyTestDeframer& operator=(const SpdyTestDeframer&) = delete;
};

// CollectedFrame holds the result of one call to SpdyDeframerVisitorInterface,
// as recorded by DeframerCallbackCollector.
struct CollectedFrame {
  CollectedFrame();
  CollectedFrame(CollectedFrame&& other);
  ~CollectedFrame();
  CollectedFrame& operator=(CollectedFrame&& other);

  // Compare a SpdyFrameIR sub-class instance, expected_ir, against the
  // collected SpdyFrameIR.
  template <class T,
            typename X =
                typename std::enable_if<std::is_base_of<SpdyFrameIR, T>::value>>
  ::testing::AssertionResult VerifyHasFrame(const T& expected_ir) const {
    return VerifySpdyFrameIREquals(expected_ir, frame_ir.get())
               ? ::testing::AssertionSuccess()
               : ::testing::AssertionFailure();
  }

  // Compare the collected headers against a StringPairVector. Ignores
  // this->frame_ir.
  ::testing::AssertionResult VerifyHasHeaders(
      const StringPairVector& expected_headers) const;

  // Compare the collected settings (parameter and value pairs) against
  // expected_settings. Ignores this->frame_ir.
  ::testing::AssertionResult VerifyHasSettings(
      const SettingVector& expected_settings) const;

  std::unique_ptr<SpdyFrameIR> frame_ir;
  std::unique_ptr<StringPairVector> headers;
  std::unique_ptr<SettingVector> settings;
  bool error_reported = false;
};

// Creates a CollectedFrame instance for each callback, storing it in the
// vector provided to the constructor.
class DeframerCallbackCollector : public SpdyDeframerVisitorInterface {
 public:
  explicit DeframerCallbackCollector(
      std::vector<CollectedFrame>* collected_frames);
  ~DeframerCallbackCollector() override {}

  void OnAltSvc(std::unique_ptr<SpdyAltSvcIR> frame_ir) override;
  void OnData(std::unique_ptr<SpdyDataIR> frame_ir) override;
  void OnGoAway(std::unique_ptr<SpdyGoAwayIR> frame_ir) override;
  void OnHeaders(std::unique_ptr<SpdyHeadersIR> frame_ir,
                 std::unique_ptr<StringPairVector> headers) override;
  void OnPing(std::unique_ptr<SpdyPingIR> frame_ir) override;
  void OnPingAck(std::unique_ptr<SpdyPingIR> frame_ir) override;
  void OnPriority(std::unique_ptr<SpdyPriorityIR> frame_ir) override;
  void OnPushPromise(std::unique_ptr<SpdyPushPromiseIR> frame_ir,
                     std::unique_ptr<StringPairVector> headers) override;
  void OnRstStream(std::unique_ptr<SpdyRstStreamIR> frame_ir) override;
  void OnSettings(std::unique_ptr<SpdySettingsIR> frame_ir,
                  std::unique_ptr<SettingVector> settings) override;
  void OnSettingsAck(std::unique_ptr<SpdySettingsIR> frame_ir) override;
  void OnWindowUpdate(std::unique_ptr<SpdyWindowUpdateIR> frame_ir) override;
  void OnError(http2::Http2DecoderAdapter::SpdyFramerError error,
               SpdyTestDeframer* deframer) override;

 private:
  std::vector<CollectedFrame>* collected_frames_;
};

}  // namespace test
}  // namespace spdy

#endif  // NET_THIRD_PARTY_SPDY_CORE_SPDY_DEFRAMER_VISITOR_H_
