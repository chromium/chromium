# Benchmarking Scenarios

This directory contains HTML-based scenarios for the frame generator. Each
scenario is designed to stress different aspects of a video encoder.

## Standard Scenarios

* **`desktop_clock`**: A simple clock on a static background.
  Useful for testing very low-bitrate "idle" states.
* **`scroll_vertical`**: Simulates a browser window scrolling vertically
  through a text-heavy page.
* **`scroll_horizontal`**: Simulates a browser window scrolling horizontally
  through a text-heavy page.
* **`moving_window`**: A static desktop with a single window moving across it.
  Tests localized region updates and "active map" logic.

## Optimization & Stress Scenarios

* **`low_motion`**: A perfectly deterministic, slow-moving UI scroll. Used as a
  baseline for rate-control stability and quality-per-bitrate measurements.
* **`high_motion`**: Large, fast-moving blocks with high-contrast interior
  details. Stresses the Motion Estimation (ME) search range and
  inter-prediction engines.
* **`chaos_stress`**: A high-entropy test with 150+ rotating, semi-transparent
  shapes moving at high velocities. Designed to maximize residual error and
  force the encoder to its limits.

## Mixed Scenarios

* **`busy_developer`**: A complex scenario with scrolling background text,
  moving shapes, and a moving terminal window. Simulates a typical
  high-activity developer environment.
