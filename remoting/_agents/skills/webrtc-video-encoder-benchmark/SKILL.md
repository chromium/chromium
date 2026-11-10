---
name: webrtc-video-encoder-benchmark
description: >-
  Run the Chrome Remote Desktop WebRTC video encoder benchmark
  (webrtc_video_encoder_benchmark) across synthetic scenarios, landscape and
  portrait resolutions, and VP8/VP9/AV1 encoder configurations, then generate
  the structured Markdown analysis report and reference-styled Google Sheet.
---

# CRD WebRTC Video Encoder Benchmark & Reporting Skill

Use this skill whenever the user asks to benchmark Chrome Remote Desktop video
encoders (`WebrtcVideoEncoderVpx`, `WebrtcVideoEncoderAV1`), evaluate codec or
rate-control changes (`EncoderBitrateFilter`, `VideoEncoderActiveMap`), or
generate a benchmark report / Google Sheet.

## 1. Build Configuration & Compilation

`//remoting/codec:webrtc_video_encoder_benchmark` is only built when
`!is_component_build && (is_win || is_linux)`. Benchmarking **must** use an
optimized Release static configuration so software encoder timings (`libvpx`,
`libaom`, `libyuv`) reflect production host performance:

1. Verify `args.gn` in the selected output directory (e.g., `out/Release`):
   ```gn
   is_debug = false
   is_component_build = false
   dcheck_always_on = false
   ```
2. Build both `webrtc_video_encoder_benchmark` and `chrome` (needed by
   [`HeadlessFrameGenerator`][headless-gen] when generating synthetic HTML
   scenario frames via Headless Chrome):
   ```bash
   autoninja -C out/Release webrtc_video_encoder_benchmark chrome
   ```

## 2. Configuring & Running `webrtc_video_encoder_benchmark`

[`webrtc_video_encoder_benchmark.cc`][benchmark-cc] accepts all workload and
encoder parameters via a JSON config file (`--config=<path>`) and writes
structured telemetry to `--output-json=<path>`. Legacy per-parameter CLI
switches (`--codec`, `--sizes`, `--scenarios`, etc.) are rejected.

### Baseline Configuration Template

Use [`references/baseline_benchmark_config.json`][baseline-config] as the
starting template for full baseline runs. It covers:

- **8 Synthetic HTML Scenarios** (defined in
  [`//remoting/test/frame_generator/scenarios/`][scenarios-readme]):
  `desktop_clock`, `scroll_vertical`, `scroll_horizontal`, `moving_window`,
  `busy_developer`, `low_motion`, `high_motion`, `chaos_stress`.
- **1 Composite Real-World Multi-Regime Workflow Scenario**
  (`desktop_workflow`, built into
  [`webrtc_video_encoder_benchmark.cc`][benchmark-cc]; `"frames" >= 180`
  recommended at 30 FPS so idle phases include 1 Hz clock ticks): runs 6
  consecutive regimes on a single persistent encoder instance without resetting
  rate-control or active-map state between phases:
  1. *Idle Desktop (`desktop_clock`, 0–20%)*: initial keyframe, quality top-off
     to `QP=10`, and post-top-off idle empty-frame throttling (plus 1 Hz
     `<2.5%` clock ticks when `frames / fps > 5`, e.g., `frames >= 180` at
     30 FPS).
  2. *App Launch & Vertical Scroll (`scroll_vertical`, 20–45%)*.
  3. *Reading Pause / Simulated Top-Off (45–55%)*: zero-damage frames
     (`updated_region().is_empty() == true`) simulating
     [`WebrtcVideoEncoderWrapper::ExtrapolateFrame()`][encoder-wrapper-cc]
     (`clear_active_map = false`) while `top_off_active_ == true`, then
     dropping post-top-off zero-damage frames up to `kKeepAliveInterval` (`2s`)
     once `kMinQuantizer` (`10`) is reached (matching
     [`WebrtcVideoEncoderWrapper::Encode()`][encoder-wrapper-cc]).
  4. *Window Drag / Multitasking (`moving_window`, 55–75%)*.
  5. *Full-Screen Burst (`chaos_stress`, 75–85%)*.
  6. *Clear Desktop & Return to Idle Recovery (`desktop_clock`, 85–100%)*:
     scene switch back to idle desktop, quality top-off, empty-frame
     throttling, and 1 Hz `<2.5%` clock ticks.
- **6 Resolutions (Landscape & Portrait)**:
  `1920x1080`, `1080x1920`, `2560x1440`, `1440x2560`, `3840x2160`, `2160x3840`.
- **7 Standard Encoder Configurations** (at fastest production real-time
  speeds):
  - `vp8_p0` (`codec: "vp8"`, `profile: 0`, fixed speed `16`)
  - `vp9_p0_s9` (`codec: "vp9"`, `profile: 0`, `encoder_speed: 9`)
  - `vp9_p1_s9` (`codec: "vp9"`, `profile: 1`, `encoder_speed: 9`)
  - `av1_p0_s11_no_am` (`codec: "av1"`, `profile: 0`, `encoder_speed: 11`,
    `use_active_map: false`)
  - `av1_p0_s11_am` (`codec: "av1"`, `profile: 0`, `encoder_speed: 11`,
    `use_active_map: true`)
  - `av1_p1_s11_no_am` (`codec: "av1"`, `profile: 1`, `encoder_speed: 11`,
    `use_active_map: false`)
  - `av1_p1_s11_am` (`codec: "av1"`, `profile: 1`, `encoder_speed: 11`,
    `use_active_map: true`)

### Executing the Benchmark

Run from the Chromium `src/` root so `base::DIR_SRC_TEST_DATA_ROOT` resolves
[`//remoting/test/frame_generator/frame_generator.py`][frame-generator-py]:

```bash
./out/Release/webrtc_video_encoder_benchmark \
    --config=/tmp/benchmark_config.json \
    --chrome-path=out/Release/chrome \
    --output-json=/tmp/benchmark_results.json
```

Note that `webrtc_video_encoder_benchmark` generates and preloads the PNG frame
sequence once per `(scenario, size)` pair and reuses those in-memory frames
across all configured encoders. Across all scenarios, once quality top-off
reaches `kMinQuantizer` (`10`), zero-damage frames are dropped up to
`kKeepAliveInterval` (`2s`) to match `WebrtcVideoEncoderWrapper::Encode()`;
each run reports `frames_captured`, `frames_encoded`, and
`empty_frames_dropped` in the JSON output (with `achieved_bitrate_kbps`
computed over the total `frames_captured` stream duration).

## 3. Generating the Markdown Analysis Report

When generating a Markdown analysis report from `benchmark_results.json`,
follow the structure in [`references/example_baseline_report.md`][example-md]
and include the following sections:

1. **Provenance Header**:
   - **CRD Host Build Version**: Read `MAJOR.MINOR.BUILD.PATCH` from
     [`//chrome/VERSION`](//chrome/VERSION) (e.g., `159.0.8130.0`) alongside
     the git commit hash.
   - **Host Environment**: CPU model (`/proc/cpuinfo`), core/thread count, and
     workstation hostname.
   - **Workload Matrix**: Scenarios $\times$ resolutions $\times$ encoders and
     total encoded frames.
2. **Executive Summary & Key Findings**:
   - **30 FPS (`33.33 ms`) Real-Time Frame Budget Compliance**: Flag any
     `(scenario, size, encoder)` run where `latency_delta.mean_ms > 33.33`.
   - **Portrait vs. Landscape Asymmetry**: Compare `1920x1080` vs. `1080x1920`,
     `2560x1440` vs. `1440x2560`, and `3840x2160` vs. `2160x3840`.
   - **4K Keyframe Encode Stalls**: Compare `latency_keyframe.max_ms` and
     `frame_size_keyframe.avg_bytes`.
   - **Compression Efficiency & Rate Control**: Compare `achieved_bitrate_kbps`
     vs. `target_bitrate_kbps` and `quantizer_delta`.
   - **Active Map (`no_am` vs. `am`)**: Compare latency and bitrate between
     `use_active_map: false` and `use_active_map: true`.
   - **Post-Run Decoded Image Quality (`quality` telemetry)**:
     `webrtc_video_encoder_benchmark` automatically decodes all encoded frames
     out-of-band *after* the timed encode loop completes (using `dav1d` for AV1
     and `libvpx` for VP8/VP9) and compares them against the source
     `DesktopFrame` via `libyuv`:
     - `mean_psnr_db`, `min_psnr_db`, and `final_frame_psnr_db` (full-frame
       YUV PSNR; `128.0 dB` = bit-exact lossless).
     - `dirty_region_mean_psnr_db` and `dirty_region_min_psnr_db` (Y-plane
       PSNR computed strictly over delta-frame `updated_region()` rectangles,
       preventing unchanged background pixels or initial keyframe CBR
       quantization from masking skipped active-map blocks or dirty-rect
       conversion bugs).
     - `mean_ssim`, `min_ssim`, and `final_frame_ssim` (Y-plane structural
       similarity).
   - **Real-World Regime & Visual Regression Gate (When Evaluating
     Optimizations)**:
     - Never rely on encoder-reported `quantizer` (`avg_qp`) alone: a buggy
       active map or dirty-rect alignment that skips modified macroblocks will
       still report `QP = 10` while leaving stale/torn pixels on screen.
     - Always run `"desktop_workflow"` alongside individual scenarios and
       verify that `dirty_region_min_psnr_db`, `final_frame_psnr_db`, and
       `mean_ssim` do not regress across regime transitions.
     - Distinguish between **adaptive / structural optimizations** (which
       adapt per-frame across all regimes) and **static coding-tool pruning**
       (e.g., permanently disabling AV1 Identity Transform `IDTX` or `CDEF`,
       which can degrade mixed text/UI/video content even if a single
       synthetic scenario speeds up).
3. **Cross-Scenario Architectural Comparisons**:
   - Include Mermaid `xychart-beta` bar charts and summary comparison tables.
4. **Complete Per-Scenario Results Tables**:
   - Link each scenario to its HTML file under
     `//remoting/test/frame_generator/scenarios/<scenario>.html`.
   - Separate Landscape (`1920x1080`, `2560x1440`, `3840x2160`) and Portrait
     (`1080x1920`, `1440x2560`, `2160x3840`) tables for each scenario.

## 4. Generating the Google Sheet Report

When the user asks for a Google Sheet report, follow the exact layout,
coordinates, formulas, and color palette documented in
[`references/google_sheet_layout_spec.md`][sheet-spec]:

1. **Use Chrome BUILD Version (`//chrome/VERSION`)**: Populate the
   `CRD Host version` row with `MAJOR.MINOR.BUILD.PATCH` (e.g.,
   `159.0.8130.0`), **never** a git commit hash.
2. **Link & Embed Scenario HTML on Each Tab**:
   - On each scenario tab (`1. desktop_clock` .. `8. chaos_stress`), set cell
     `A7` to a clickable `=HYPERLINK(...)` pointing to
     `https://source.chromium.org/chromium/chromium/src/+/main:remoting/test/frame_generator/scenarios/<scenario>.html`.
   - Attach a **Cell Note** (`note` field via `repeatCell`) on `A7` containing
     the scenario's `.html` source code so hovering over `A7` previews the
     HTML/JS inline.
   - Include a `Scenario HTML File` hyperlink column (`Col F`) in the
     `Test Scenario Info` table on `Summary & Overview`.
3. **Side-by-Side Resolution Blocks & Color Palette**:
   - Place Landscape resolutions (`1920x1080` in `Cols B–H`, `2560x1440` in
     `Cols J–P`, `3840x2160` in `Cols R–X`) side-by-side at the top, and stack
     Portrait resolutions (`1080x1920`, `1440x2560`, `2160x3840`) directly
     beneath them in the same columns.
   - Apply the exact encoder column header fills (`VP8` Cyan `#00FFFF`,
     `VP9 P0` Yellow `#FFFF00`, `VP9 P1` Light Magenta `#EAD1DC`, `AV1 P0`
     Light Blue `#9FC5E8` / Cornflower Blue `#7FA2E1`, `AV1 P1` Light Green
     `#D9EAD3` / Sage Green `#B6D7A8`) and separator bars (Dark Gray `#595959`
     and Medium Blue `#4A86E8`) specified in
     [`references/google_sheet_layout_spec.md`][sheet-spec].

[baseline-config]: ./references/baseline_benchmark_config.json
[benchmark-cc]: //remoting/codec/webrtc_video_encoder_benchmark.cc
[encoder-wrapper-cc]: //remoting/protocol/webrtc_video_encoder_wrapper.cc
[example-md]: ./references/example_baseline_report.md
[frame-generator-py]: //remoting/test/frame_generator/frame_generator.py
[headless-gen]: //remoting/test/frame_generator/headless_frame_generator.cc
[scenarios-readme]: //remoting/test/frame_generator/scenarios/README.md
[sheet-spec]: ./references/google_sheet_layout_spec.md
