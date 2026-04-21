# Remoting Codec Instructions

Instructions for working in `//remoting/codec`.

## Architecture Overview

The `codec` directory contains the audio and video encoding/decoding logic.
Chrome Remote Desktop has migrated to WebRTC-based video encoding; much of the
non-WebRTC code in this directory is obsolete and scheduled for deletion.

### Video Codecs

#### Current Implementation (WebRTC-based)
CRD implements custom wrappers around WebRTC encoders, optimized for low-latency
desktop streaming.
*   **Base Interface:** `WebrtcVideoEncoder`
*   **Active Encoders:**
    *   `WebrtcVideoEncoderVpx`: Handles VP8 and VP9 encoding.
    *   `WebrtcVideoEncoderAv1`: Handles AV1 encoding. This is the default codec
        used for CRD sessions.
*   **Experimental / Unsupported:**
    *   `WebrtcVideoEncoderGpu`: Hardware-accelerated encoding. It is compiled
        on Windows and Linux, but is not currently supported for general usage.

### Audio Codecs
*   **Opus:** CRD exclusively uses Opus for audio streaming.
*   **Implementation:** WebRTC is currently used for Opus encoding and decoding.

### Utilities
*   `ScopedVpxCodec`: A helper for managing the lifetime of `vpx_codec_ctx`.

## Key Files to Read
*   `remoting/codec/webrtc_video_encoder.h`: The primary video encoding
    interface.
*   `remoting/codec/webrtc_video_encoder_aom.h`: Provides the AV1 encoder which
    is the default and most commonly used video encoder implementation.
*   `remoting/codec/webrtc_video_encoder_vpx.h`: Provides the VP8 and VP9
    encoders which are used for fallback or compatibility.
