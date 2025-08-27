# WebCodecs API

This directory contains the Blink implementation of the [WebCodecs API](https://www.w3.org/TR/webcodecs/).
It provides low-level access to browser media encoders and decoders, both software and GPU-accelerated.

## Core Interfaces

The main IDL interfaces of the API and their corresponding C++ implementation

*   **`VideoFrame`**: (`video_frame.h/.cc`) - A wrapper around `media::VideoFrame` for use in Blink and JavaScript.
*   **`AudioData`**: (`audio_data.h/.cc`) - A wrapper around `media::AudioBuffer` for uncompressed audio data.
*   **`EncodedVideoChunk`**: (`encoded_video_chunk.h/.cc`) - A wrapper for a `media::DecoderBuffer` containing a chunk of encoded video.
*   **`EncodedAudioChunk`**: (`encoded_audio_chunk.h/.cc`) - A wrapper for a `media::DecoderBuffer` containing a chunk of encoded audio.
*   **`VideoDecoder`**: (`video_decoder.h/.cc`) - Manages the video decoding process by calling `media::VideoDecoder`s.
*   **`VideoEncoder`**: (`video_encoder.h/.cc`) - Manages the video encoding process by calling `media::VideoEncoder`s.
*   **`AudioDecoder`**: (`audio_decoder.h/.cc`) - Manages the audio decoding process by calling `media::AudioDecoder`s.
*   **`AudioEncoder`**: (`audio_encoder.h/.cc`) - Manages the audio encoding process by calling `media::AudioEncoder`s.
*   **`ImageDecoder`**: (`image_decoder_external.h/.cc`) - Manages decoding of image formats (e.g., PNG, JPEG, GIF).