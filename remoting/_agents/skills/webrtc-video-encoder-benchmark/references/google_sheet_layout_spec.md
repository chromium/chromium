# CRD Video Encoder Benchmark — Spreadsheet Layout & Styling Specification

This document defines the canonical layout, cell coordinates, formulas, and
visual styling for Chrome Remote Desktop WebRTC video encoder benchmark
spreadsheets (Google Sheets / XLSX).

## 1. Spreadsheet Structure & API Guidelines

- **Batch Operations**: Prefer batched write (`values.batchUpdate`) and
  formatting (`spreadsheets.batchUpdate`) operations rather than per-cell reads
  or writes to avoid rate limits.
- **Non-Zero `sheetId` Rule**: When adding tabs via `addSheet` in a
  `BatchUpdateSpreadsheetRequest`, **never** pass `"sheetId": 0` (proto3 treats
  `0` as unset and assigns a random server-generated ID). Always use explicit
  non-zero IDs (e.g., `"sheetId": 10` for `Summary & Overview`, and
  `"sheetId": 1..8` for `1. desktop_clock` through `8. chaos_stress`).

## 2. Tab Structure (9 Tabs)

Configure every tab with `columnCount: 25` (`Cols A..Y`), `rowCount: 65`, and
`frozenColumnCount: 1` (`Col A` frozen):

| Tab Index | `sheetId` | Tab Title |
| :---: | :---: | :--- |
| `0` | `10` | `Summary & Overview` |
| `1` | `1` | `1. desktop_clock` |
| `2` | `2` | `2. scroll_vertical` |
| `3` | `3` | `3. scroll_horizontal` |
| `4` | `4` | `4. moving_window` |
| `5` | `5` | `5. busy_developer` |
| `6` | `6` | `6. low_motion` |
| `7` | `7` | `7. high_motion` |
| `8` | `8` | `8. chaos_stress` |

## 3. Side-by-Side Column Layout (`Cols A..X`)

Each tab arranges three resolution groups side-by-side horizontally, separated
by narrow blank spacer columns (`Col I` and `Col Q`):

- **`Col A` (`index 0`, width `225px`)**: Row labels (right-aligned, bold in
  header and metric sections) and top metadata block (`Rows 3–7`, left-aligned,
  bold).
- **Resolution Group 1 (`Cols B..H`, `indices 1..8`, width `105px` each)**:
  `1920x1080` (Landscape block) and `1080x1920` (Portrait block).
- **Spacer 1 (`Col I`, `index 8`, width `32px`)**: Blank separator column.
- **Resolution Group 2 (`Cols J..P`, `indices 9..16`, width `105px` each)**:
  `2560x1440` (Landscape block) and `1440x2560` (Portrait block).
- **Spacer 2 (`Col Q`, `index 16`, width `32px`)**: Blank separator column.
- **Resolution Group 3 (`Cols R..X`, `indices 17..24`, width `105px` each)**:
  `3840x2160` (Landscape block) and `2160x3840` (Portrait block).

Within each 7-column resolution block (`B..H`, `J..P`, `R..X`), the columns
appear in this exact order:

| Offset | Encoder Config | `Encoder type/profile` | `Profile` | `Encoder speed` | Header Fill Hex | Header RGB (`0.0–1.0`) |
| :---: | :--- | :---: | :---: | :---: | :---: | :--- |
| `+0` (`B, J, R`) | `vp8_p0` | `VP8` | `Profile-0` | `16` | `#00FFFF` (Cyan) | `{red: 0.0, green: 1.0, blue: 1.0}` |
| `+1` (`C, K, S`) | `vp9_p0_s9` | `VP9` | `Profile-0` | `9` | `#FFFF00` (Yellow) | `{red: 1.0, green: 1.0, blue: 0.0}` |
| `+2` (`D, L, T`) | `vp9_p1_s9` | `VP9` | `Profile-1` | `9` | `#EAD1DC` (Light Magenta) | `{red: 0.918, green: 0.820, blue: 0.863}` |
| `+3` (`E, M, U`) | `av1_p0_s11_no_am` | `AV1` | `Profile-0` | `11` | `#9FC5E8` (Light Blue) | `{red: 0.624, green: 0.773, blue: 0.910}` |
| `+4` (`F, N, V`) | `av1_p0_s11_am` | `AV1` | `Profile-0 (AM)` | `11` | `#7FA2E1` (Cornflower Blue) | `{red: 0.498, green: 0.698, blue: 0.882}` |
| `+5` (`G, O, W`) | `av1_p1_s11_no_am` | `AV1` | `Profile-1` | `11` | `#D9EAD3` (Light Green) | `{red: 0.851, green: 0.918, blue: 0.827}` |
| `+6` (`H, P, X`) | `av1_p1_s11_am` | `AV1` | `Profile-1 (AM)` | `11` | `#B6D7A8` (Sage Green) | `{red: 0.714, green: 0.843, blue: 0.659}` |

## 4. Row Layout for Scenario Tabs (`1. desktop_clock` .. `8. chaos_stress`)

### Top Metadata Block (`Rows 3–7`, `Col A`)
- **`A3`**: `CPU: <CPU model string>`
- **`A4`**: `Host OS / Machine: <OS and architecture>`
- **`A5`**: `Harness: webrtc_video_encoder_benchmark (Release static, is_debug=false)`
- **`A6`**: `Scenario <N>: <scenario_name> — <Description> (<Updated Area>% Updated Area)`
- **`A7`**: Clickable `=HYPERLINK("https://source.chromium.org/chromium/chromium/src/+/main:remoting/test/frame_generator/scenarios/<scenario>.html", "Scenario HTML: //remoting/test/frame_generator/scenarios/<scenario>.html")`
  - **Embedded Source Note**: Attach a **Cell Note** (`repeatCell` `note` field)
    on `A7` containing the HTML/CSS/JS source of
    `//remoting/test/frame_generator/scenarios/<scenario>.html` so hovering
    over `A7` previews the scenario code inline.

### Block 1: Landscape Resolutions (`1920x1080`, `2560x1440`, `3840x2160`)
- **Stacked 6-Row Header (`Rows 8–13`, 0-based `7..13`)**:
  - **`Row 8`**: `CRD Host version` — **MUST** use the Chrome `MAJOR.MINOR.BUILD.PATCH`
    version read from `//chrome/VERSION` (e.g., `159.0.8130.0`), **not** a git
    commit hash.
  - **`Row 9`**: `Encoder type/profile` (`VP8`, `VP9`, `AV1`)
  - **`Row 10`**: `Profile` (`Profile-0`, `Profile-1`, `Profile-0 (AM)`, `Profile-1 (AM)`)
  - **`Row 11`**: `Encoder speed` (`16`, `9`, `11`)
  - **`Row 12`**: `FPS target` (`30.0`)
  - **`Row 13`**: `Resolution` (`1920x1080` in `B..H`, `2560x1440` in `J..P`, `3840x2160` in `R..X`)
- **Dark Gray Separator Bar (`Row 14`, 0-based `13..14`, height `12px`)**:
  - Fill `B14:H14`, `J14:P14`, `R14:X14` with Dark Gray (`#595959`,
    `{red: 0.35, green: 0.35, blue: 0.35}`).
- **Headline Latency Rows (`Rows 15–16`, 0-based `14..16`, bold, `0.00` format)**:
  - **`Row 15`**: `Average Encode Time (ms)` (`latency_delta.mean_ms`)
  - **`Row 16`**: `Median encode time (ms)` (`latency_delta.p50_ms`)
- **Medium Blue Separator Bar (`Row 17`, 0-based `16..17`, height `12px`)**:
  - Fill `B17:H17`, `J17:P17`, `R17:X17` with Medium Blue (`#4A86E8`,
    `{red: 0.290, green: 0.525, blue: 0.910}`).
- **Detailed Telemetry Rows (`Rows 18–26`, 0-based `17..26`)**:
  - **`Row 18`**: `P90 encode time (ms)` (`latency_delta.p90_ms`, `0.00`)
  - **`Row 19`**: `P99 encode time (ms)` (`latency_delta.p99_ms`, `0.00`)
  - **`Row 20`**: `Keyframe encode time (ms)` (`latency_keyframe.max_ms`, `0.00`)
  - **`Row 21`**: `All frames avg encode time (ms)` (`latency_all.mean_ms`, `0.00`)
  - **`Row 22`**: `Encoder throughput (FPS)` (`encoder_fps`, `0.0`)
  - **`Row 23`**: `Target bitrate (kbps)` (`target_bitrate_kbps`, `#,##0`)
  - **`Row 24`**: `Achieved bitrate (kbps)` (`achieved_bitrate_kbps`, `0.0`)
  - **`Row 25`**: `Delta QP (Avg / Max)` (`"<avg_qp> / <max_qp>"` from `quantizer_delta`)
  - **`Row 26`**: `Updated screen area (%)` (`updated_area_percent`, `0.00`)

### Block 2: Portrait Resolutions (`1080x1920`, `1440x2560`, `2160x3840`)
- **`Rows 27–28`**: Blank separator rows.
- **`Rows 29–34` (0-based `28..34`)**: Stacked 6-row header (`Resolution` =
  `1080x1920` in `B..H`, `1440x2560` in `J..P`, `2160x3840` in `R..X`).
- **`Row 35` (0-based `34..35`, height `12px`)**: Dark Gray separator bar (`#595959`).
- **`Rows 36–37` (0-based `35..37`)**: `Average Encode Time (ms)` and
  `Median encode time (ms)` (bold, `0.00`).
- **`Row 38` (0-based `37..38`, height `12px`)**: Medium Blue separator bar (`#4A86E8`).
- **`Rows 39–47` (0-based `38..47`)**: Detailed telemetry rows matching `Rows 18–26`.

> **Formatting Tip**: Format Block 1 on `sheetId: 1` first, then use `copyPaste`
> with `pasteType: "PASTE_FORMAT"` to copy Block 1's formatting onto Block 2 of
> `sheetId: 1` (`startRowIndex: 7..26 -> 28..47`), and then copy the entire
> sheet formatting (`startRowIndex: 0..48`) from `sheetId: 1` to `sheetId: 2..8`.

## 5. Row Layout for `Summary & Overview` Tab (`sheetId: 10`)

- **`Rows 3–6` (`Col A`)**: Top metadata block (`CPU`, `Host OS / Machine`,
  `Harness`, `Baseline Suite`).
- **Landscape Summary Block (`Rows 8–24`, 0-based `7..24`)**:
  - `Rows 8–13`: Stacked 6-row header (`1920x1080`, `2560x1440`, `3840x2160`).
  - `Row 14`: Dark Gray separator bar (`#595959`, height `12px`).
  - `Row 15`: `Overall Average Encode Time (ms)` using `=AVERAGE(B17:B24)`
    through `=AVERAGE(X17:X24)` (bold, `0.00`).
  - `Row 16`: Medium Blue separator bar (`#4A86E8`, height `12px`).
  - `Rows 17–24`: Per-scenario `Average Encode Time (ms)` rows
    (`1. desktop_clock (ms)` through `8. chaos_stress (ms)`).
- **Portrait Summary Block (`Rows 27–43`, 0-based `26..43`)**:
  - `Rows 27–32`: Stacked 6-row header (`1080x1920`, `1440x2560`, `2160x3840`).
  - `Row 33`: Dark Gray separator bar (`#595959`, height `12px`).
  - `Row 34`: `Overall Average Encode Time (ms)` using `=AVERAGE(B36:B43)`
    through `=AVERAGE(X36:X43)` (bold, `0.00`).
  - `Row 35`: Medium Blue separator bar (`#4A86E8`, height `12px`).
  - `Rows 36–43`: Per-scenario Portrait `Average Encode Time (ms)` rows.
- **`Test Scenario Info` Table (`Rows 46–54`, `Cols A..F`, 0-based `45..54`)**:
  - `Row 46` Header (Light Green `#D9EAD3`, bold):
    `["Test Scenario Info", "Frames", "FPS Target", "Updated Area (%)", "Workload Description", "Scenario HTML File"]`
  - `Rows 47–54`: Scenario metadata rows with clickable `=HYPERLINK(...)` in
    `Col F` pointing to each scenario's `.html` file in Chromium Code Search.
