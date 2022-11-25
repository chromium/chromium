# Customization Guide - Fingerprint Diagnostics

# Objective
Provide the instructions on the fingerprint diagnostic routines on ChromeOS
devices.

For an overview of the Telemetry Extension platform, please visit our
[main documentation](README.md).

[TOC]

# Instructions
This section provides the instructions on how to configure fingerprint
diagnostic routines parameters with or without Boxster.

> **_NOTE:_** It is your responsibility to make sure the parameters you fill in
are correct. Neither cros-config nor Boxster validate that value.

## Configure fingerprint parameters
### Models with Boxster

1. In `config.star`
    1. Find a variable initialized with `hw_topo.create_fingerprint`.
    2. Add a keyword argument `fingerprint_diag` and fill in all the
[fingerprint parameters](#fingerprint-parameters). Those parameters should be
found in the fingerprint module spec.
2. Reflect your changes in `generated/config.jsonproto` and
`sw_build_config/platform/chromeos-config/generated/project-config.json` by
running: `./config/bin/gen_config config.star`.
3. Commit your changes. Sample CL:
[crrev.com/c/3385463](http://crrev.com/c/3385463).

The configuration in  `config.star` would look like this:
```python
// config.star
_FINGERPRINT = hw_topo.create_fingerprint(
    "FINGERPRINT",
    "Default fingerprint",
    location = hw_topo.fp_loc.KEYBOARD_BOTTOM_LEFT,
    board = "fake_fingerprint_board",
    fingerprint_diag = hw_topo.create_fingerprint_diag(
        routine_enable = True,
        max_pixel_dev = 5,
        max_dead_pixels = 5,
        pixel_median = hw_topo.create_fingerprint_diag_pixel_median(
            cb_type1_lower = 1,
            cb_type1_upper = 2,
            cb_type2_lower = 3,
            cb_type2_upper = 4,
            icb_type1_lower = 5,
            icb_type1_upper = 6,
            icb_type2_lower = 7,
            icb_type2_upper = 8,
        ),
        num_detect_zone = 2,
        detect_zones = [hw_topo.create_fingerprint_diag_detect_zone(
            x1 = 10,
            y1 = 20,
            x2 = 30,
            y2 = 40,
        ), hw_topo.create_fingerprint_diag_detect_zone(
            x1 = 50,
            y1 = 60,
            x2 = 70,
            y2 = 80,
        )],
        max_dead_pixels_in_detect_zone = 0,
        max_reset_pixel_dev = 5,
        max_error_reset_pixels = 5,
    ),
)
```

### Models without Boxster

1. In `model.yaml`
    1. Find the config of the model you want to enable.
    2. (skip if existed) Add `cros-healthd` section under the config.
    3. (skip if existed) Add `routines` section under `cros-healthd` section.
    4. Add `fingerprint-diag` under `routines` section.
    5. Add [fingerprint parameters](#fingerprint-parameters) under
`fingerprint-diag` section.
    6. Commit your changes and upload them to the internal Gerrit.

An example `model.yaml` could look somewhat like this:
```yaml
xxx-config:
  ...
  cros-healthd:
    routines:
      fingerprint-diag:
        routine-enable: True
        max-dead-pixels: 10
        max-dead-pixels-in-detect-zone: 0
        max-pixel-dev: 25
        num-detect-zone: 2
        max-error-reset-pixels: 5
        max-reset-pixel-dev: 55
        pixel-median:
          cb-type1-lower: 180
          cb-type1-upper: 220
          cb-type2-lower: 80
          cb-type2-upper: 120
          icb-type1-lower: 15
          icb-type1-upper: 70
          icb-type2-lower: 145
          icb-type2-upper: 200
        detect-zones: [
          {x1: 20, y1: 16, x2: 27, y2: 23},
          {x1: 76, y1: 16, x2: 83, y2: 23},
          {x1: 132, y1: 16, x2: 139, y2: 23},
          {x1: 20, y1: 56, x2: 27, y2: 63},
          {x1: 76, y1: 56, x2: 83, y2: 63},
          {x1: 132, y1: 56, x2: 139, y2: 63},
          {x1: 20, y1: 88, x2: 27, y2: 95},
          {x1: 76, y1: 88, x2: 83, y2: 95},
          {x1: 132, y1: 88, x2: 139, y2: 95},
          {x1: 20, y1: 128, x2: 27, y2: 135},
          {x1: 76, y1: 128, x2: 83, y2: 135},
          {x1: 132, y1: 128, x2: 139, y2: 135}
        ]
  ...
```

## Fingerprint parameters
| Attribute | Type   | RegEx     | Required | Oneof Group | Build-only | Description |
| --------- | ------ | --------- | -------- | ----------- | ---------- | ----------- |
| detect-zones | array - [detect-zones](#detect_zones) |  | False |  | False | Rectangles [x1, y1, x2, y2].  |
| max-dead-pixels | integer |  | True |  | False | The maximum allowed number of dead pixels on the fingerprint sensor.  Minimum value: 0x0. |
| max-dead-pixels-in-detect-zone | integer |  | True |  | False | The maximum allowed number of dead pixels in the detection zone.  Minimum value: 0x0. |
| max-error-reset-pixels | integer |  | True |  | False | The maximum allowed number of error pixels when doing the reset test on the fingerprint sensor.  Minimum value: 0x0. |
| max-pixel-dev | integer |  | True |  | False | The maximum deviation from the median for a pixel.  Minimum value: 0x0. |
| max-reset-pixel-dev | integer |  | True |  | False | The maximum deviation from the median for a pixel when doing the reset test.  Minimum value: 0x0. |
| num-detect-zone | integer |  | True |  | False | The number of detect zone.  Minimum value: 0x0. |
| pixel-median | [pixel-median](#pixel_median) |  | True |  | False | Range constraints of the pixel median value of the checkerboards.  |
| routine-enable | boolean |  | True |  | False | Enable fingerprint diagnostic routine or not.  |

### detect-zones
| Attribute | Type   | RegEx     | Required | Oneof Group | Build-only | Description |
| --------- | ------ | --------- | -------- | ----------- | ---------- | ----------- |
| x1 | integer |  | True |  | False | `x1` should be smaller than `x2`.  Minimum value: 0x0. |
| x2 | integer |  | True |  | False | `x1` should be smaller than `x2`.  Minimum value: 0x0. |
| y1 | integer |  | True |  | False | `y1` should be smaller than `y2`.  Minimum value: 0x0. |
| y2 | integer |  | True |  | False | `y1` should be smaller than `y2`.  Minimum value: 0x0. |

### pixel-median
| Attribute | Type   | RegEx     | Required | Oneof Group | Build-only | Description |
| --------- | ------ | --------- | -------- | ----------- | ---------- | ----------- |
| cb-type1-lower | integer |  | True |  | False | Checkerboard type1 lower bound.  Minimum value: 0x0. Maximum value: 0xff. |
| cb-type1-upper | integer |  | True |  | False | Checkerboard type1 upper bound.  Minimum value: 0x0. Maximum value: 0xff. |
| cb-type2-lower | integer |  | True |  | False | Checkerboard type2 lower bound.  Minimum value: 0x0. Maximum value: 0xff. |
| cb-type2-upper | integer |  | True |  | False | Checkerboard type2 upper bound.  Minimum value: 0x0. Maximum value: 0xff. |
| icb-type1-lower | integer |  | True |  | False | Inverted checkerboard type1 lower bound.  Minimum value: 0x0. Maximum value: 0xff. |
| icb-type1-upper | integer |  | True |  | False | Inverted checkerboard type1 upper bound.  Minimum value: 0x0. Maximum value: 0xff. |
| icb-type2-lower | integer |  | True |  | False | Inverted checkerboard type2 lower bound.  Minimum value: 0x0. Maximum value: 0xff. |
| icb-type2-upper | integer |  | True |  | False | Inverted checkerboard type2 upper bound.  Minimum value: 0x0. Maximum value: 0xff. |
