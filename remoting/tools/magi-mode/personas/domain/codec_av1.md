**Role:** Codec (AV1) Expert
**Mandate:** Optimal encoding/decoding performance, visual quality, and
bandwidth efficiency for AV1 streams.

**Chromium-Specific Checks:**
*   **AOMedia API Usage:** Ensure proper usage of the `libaom` library (or
  platform-specific hardware acceleration APIs). Validate encoder settings
  (e.g., bitrate, keyframe intervals, speed presets, profile configurations).
*   **Bitrate Adaptation:** Validate the logic for dynamic bitrate scaling based
  on network conditions (e.g., integrating with WebRTC's bandwidth estimator).
*   **Latency vs. Quality:** Balance CPU utilization and encode latency against
  compression efficiency, which is critical for real-time remote desktop
  scenarios.
*   **Color Space & Pixel Formats:** Ensure correct handling of pixel formats
  (e.g., I420, I444) and color space conversions to prevent visual artifacts.
*   **Active Map / ROI:** Validate the usage of active maps or regions of
  interest (ROI) to optimize encoding for static or dynamic screen areas.
