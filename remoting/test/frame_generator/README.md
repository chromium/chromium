# Frame Generator Tool

This directory contains a standalone tool for generating synthetic frame
sequences using Headless Chrome. These sequences are used to stress-test and
benchmark video encoders (like AV1) in the Chrome Remote Desktop context.

## Requirements

**Note: This tool is currently supported on Linux and Mac only.**

## Overview

The tool uses the Chrome DevTools Protocol (CDP) to drive a headless instance
of Chrome. It loads a "scenario" (an HTML/JS file), advances time
deterministically, and captures screenshots for each frame.

## Usage

```bash
python3 remoting/test/frame_generator/frame_generator.py \
    --scenario <name> \
    --out-dir <path> \
    --frames 100 \
    --fps 30 \
    --width 1920 \
    --height 1080
```

### Arguments

* `--scenario`: The name of the HTML file in the `scenarios/` directory
  (without `.html`).
* `--out-dir`: Directory where the generated PNG frames will be saved.
* `--frames`: Number of frames to generate (default: 100).
* `--fps`: Frame rate for the generated sequence (default: 30.0).
* `--width/--height`: Dimensions of the capture window (default: 1920x1080).
* `--chrome-path`: Path to the Chrome binary (default: `out/Release/chrome`).

## Adding New Scenarios

When creating a new scenario in `scenarios/`, you must adhere to the following
technical contract:

1. **File Format**: Must be a standalone HTML file.
2. **Deterministic Timing**: You MUST implement `window.setBenchmarkTime(t)`.
   This function is called by the Python script to advance the scene to a
   specific point in time `t` (in seconds).
3. **Seeded Randomness**: Do NOT use `Math.random()`. Use a deterministic
   seeded random number generator (e.g., an LCG) to ensure identical output
   across different runs and machines.
4. **Visual Verification**: You SHOULD implement an `animate(t)` loop using
   `requestAnimationFrame` that calls your render logic. This allows the
   scenario to "auto-play" when opened manually in a browser for debugging.
5. **No External Assets**: Prefer procedurally generated content (Canvas/CSS)
   over external images to ensure the test is self-contained and loads
   instantly.

## How it Works

1. **Orchestration**: The Python script launches Chrome with
   `--remote-debugging-pipe`.
2. **Synchronization**: For each frame, the script calls a global
   `window.setBenchmarkTime(t)` function within the loaded HTML page. This
   ensures that even animations are deterministic relative to the frame index.
3. **Capture**: After the JS has updated the DOM/Canvas, the tool issues a
   `Page.captureScreenshot` command to get the raw pixel data.
