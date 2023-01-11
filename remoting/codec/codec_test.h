// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_CODEC_TEST_H_
#define REMOTING_CODEC_CODEC_TEST_H_

#include <list>

namespace webrtc {
class DesktopFrame;
class DesktopSize;
}  // namespace webrtc

namespace remoting {

class VideoDecoder;
class VideoEncoder;

// Generate test data and test the encoder for a regular encoding sequence.
// This will test encoder test and the sequence of messages sent.
//
// If |strict| is set to true then this routine will make sure the updated
// rects match dirty rects.
void TestVideoEncoder(VideoEncoder* encoder, bool strict);

// Generate test data and test the encoder for a sequence of one "changed"
// frame followed by one or more "unchanged" frames, and verify that the
// encoder sends up to |max_topoff_frames| of non-empty data for unchanged
// frames, after which it returns null frames.
void TestVideoEncoderEmptyFrames(VideoEncoder* encoder, int max_topoff_frames);

// Generate test data and test the encoder and decoder pair.
//
// If |strict| is set to true, this routine will make sure the updated rects
// are correct.
void TestVideoEncoderDecoder(VideoEncoder* encoder,
                             VideoDecoder* decoder,
                             bool strict);

// Generate a frame containing a gradient, and test the encoder and decoder
// pair.
void TestVideoEncoderDecoderGradient(VideoEncoder* encoder,
                                     VideoDecoder* decoder,
                                     const webrtc::DesktopSize& screen_size,
                                     double max_error_limit,
                                     double mean_error_limit);

// Run sufficient encoding iterations to measure the FPS of the specified
// encoder. The caller may supply one or more DesktopFrames to encode, which
// will be cycled through until timing is complete. If the caller does not
// supply any frames then a single full-frame of randomized pixels is used.
float MeasureVideoEncoderFpsWithSize(VideoEncoder* encoder,
                                     const webrtc::DesktopSize& size);
float MeasureVideoEncoderFpsWithFrames(
    VideoEncoder* encoder,
    const std::list<webrtc::DesktopFrame*>& frames);

}  // namespace remoting

#endif  // REMOTING_CODEC_CODEC_TEST_H_
