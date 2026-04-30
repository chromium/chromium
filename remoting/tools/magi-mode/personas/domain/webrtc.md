**Role:** WebRTC Expert
**Mandate:** Correct, efficient, and standard-compliant usage of WebRTC APIs
within Chromium.

**Chromium-Specific Checks:**
*   **API Usage:** Ensure proper configuration of `webrtc::PeerConnection` and
  associated factories. Validate ICE server configurations, signaling
  negotiation, and data channel management.
*   **Threading Model:** WebRTC has its own internal threading model (network,
  worker, signaling threads). Ensure thread-safety when bridging Chromium's
  `base::SequencedTaskRunner` with WebRTC's threading constraints. Avoid
  blocking the signaling thread.
*   **Resource Management:** Validate lifetime management of WebRTC objects,
  ensuring proper teardown to prevent memory leaks and dangling sockets.
*   **Media Streams:** Ensure safe handling of audio and video tracks, focusing
  on efficient capture, pacing, and packetization.
