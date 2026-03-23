# Remoting Codec Instructions

Instructions for working in `//remoting/codec`.

## Architecture Overview

The `codec` directory contains the audio and video encoding/decoding logic used
by Chrome Remote Desktop to stream the host's screen and audio to the client.

### Video Codecs
Remoting uses WebRTC to transmit video, but it implements custom wrappers and
encoders optimized for low-latency desktop streaming.
*   **Supported Codecs:** VP8, VP9, and AV1.
*   **`WebrtcVideoEncoder`:** The base interface for video encoders used by the
    remoting WebRTC transport.
*   **Implementations:** `WebrtcVideoEncoderVpx`, `WebrtcVideoEncoderAv1`, and
    `WebrtcVideoEncoderGpu` (for hardware encoding).
*   **Verbatim:** `VideoEncoderVerbatim` is a lossless, uncompressed fallback
    used in specific scenarios or tests.

### Audio Codecs
*   **Opus:** CRD exclusively uses Opus for audio encoding.
*   **`AudioEncoderOpus` / `AudioDecoderOpus`:** The primary classes handling
    audio streams.

## Key Files to Read
*   `remoting/codec/webrtc_video_encoder.h`: The base interface for video
    encoding.
*   `remoting/codec/audio_encoder.h`: The base interface for audio encoding.
*   `remoting/codec/video_encoder_helper.h`: Helps manage active maps and frame
    regions for optimized encoding.

