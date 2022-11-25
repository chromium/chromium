# Customization Guide - OEM Name

# Objective
Provide the instructions on the OEM name customization on ChromeOS devices.

For an overview of the Telemetry Extension platform, please visit our
[main documentation](README.md).

[TOC]

# Overview
We have proposed a hybrid approach "cros_config first, fallback to VPD" for OEM
name customization. This approach first tries to search for the OEM name in
cros_config and checks a VPD field if the name is not present. Right now
(October, 2022) only the cros_config part is implemented; the VPD part is in
progress. In general the OEM name field is used to differentiate between
different OEMs on a ChromeOS device. The field is currently used by the
[Telemetry Extension](README.md) and is a requirement to use the Extension.
This guide explains how to set the OEM name in cros_config.

## Rollout schedule
Please note that the changes made here follow the Chrome Release Cycle (actual
timeline can be found in
[Chromium Dashboard](https://chromiumdash.appspot.com/schedule)), which means
it will take at least 6 weeks to rollout your changes to the public.

# Instructions
This section provides the instructions on how to check OEM name config, and set
/verify the OEM name for models with or without Boxster.

> **_NOTE:_** It is your responsibility to make sure the OEM name you fill in is
correct. Neither cros-config nor Boxster validate that value.

## Check & Set OEM name
### Models with Boxster

1. In `config.star`
    1. Find a variable initialized with `device_brand.create` (See
       [Troubleshooting](#Troubleshooting) if you cannot find it). Usually named  `_DEVICE_BRAND`.
    2. Find the variable (`_FAKE_OEM` in our example) that is passed to `oem_id`
       and make sure the value inside it is correct. In case you need to modify
       the existing OEM name, you could follow the sample CL
       [crrev.com/c/3627425](http://crrev.com/c/3627425).
    3. Add a keyword argument `export_oem_info = True`.
2. Reflect your changes in `generated/config.jsonproto` and
`sw_build_config/platform/chromeos-config/generated/project-config.json` by
running: `./config/bin/gen_config config.star`.
3. Commit your changes. Sample CL:
[crrev.com/c/3385463](http://crrev.com/c/3385463).

The configuration in  `config.star` would look like this:
```python
// config.star
_FAKE_OEM = partner.create("FAKE_OEM") # Make sure it’s correct
...
_DEVICE_BRAND = device_brand.create(
brand_name = "Fake ChromeOS Device Brandname",
design_id = _DESIGN_ID,
oem_id = _FAKE_OEM.id, # This argument is required
brand_code = "AAAA",
export_oem_info = True, # Add this line
)
```

### Models without Boxster

1. in `model.yaml`
    1. Make sure inside `base-config` there’s another dictionary named `branding`
       containing an entry with key `oem-name` and value `{{$oem-name}}`.
       Optionally, you can add another entry `$oem-name` with value `""` (an
       empty string) which will be used as default value.
    2. Add a new entry `$oem-name` whose value is the OEM name you would like to
       set in `chromeos.devices[].products[]`.
    3. Commit your changes and upload them to the internal Gerrit.

An example `model.yaml` could look somewhat like this:
```yaml
base-config: &base_config
 ...
 branding:
   $oem-name: ""
   oem-name: "{{$oem-name}}"
 ...
 devices:
   ...
   - $device-name: "foo"
     $has-smart-battery-info: true
     products:
       - $brand-code: "FOO"
         $marketing-name: "TBD"
         $oem-name: "OEM_A" # This is what you set.
     skus:
       - $sku-id-val: 42
         config: *foo_config
```

## Verify the OEM name

You can verify the OEM name configuration by running the following command:
```
cros-health-tool telem --category=system
```

Check if `os_info.oem_name` is present and correct.
A sample output would look somewhat like that:
```
$ cros-health-tool telem --category=system
{
  "dmi_info": {
     ...
  },
  "os_info": {
     "boot_mode": "cros_secure",
     "code_name": "foo",
     "marketing_name": "TBD",
     "oem_name": "OEM_A",    # check this line
     ...
  },
  "vpd_info": {
     ...
  }
```

## Troubleshooting

### No device_brand.create in config.star?
In some projects, you might see `_DEVICE_BRAND` is initialized with
`program.brand`. In this case, you need to:

1. Go to `program/program.star` (note that this file is in a different repo
`chromeos/program/${BOARD}` as `program/` is a soft link) and modify the
`_brand` function:
2. Go back to `config.star` and add the parameter `export_oem_info = True` to
the `program.brand` function.
3. Regenerate config.

Sample `program/program.star`:
```python
def _brand(design_id, oem_id = None, brand_code = _DEFAULT_BRAND_CODE, brand_name = None, export_oem_info = False):
   return device_brand.create(
       brand_name = brand_name,
       design_id = design_id,
       oem_id = oem_id,
       brand_code = brand_code,
       export_oem_info = export_oem_info,
   )
```

Sample `config.star`:
```python
_DEVICE_BRAND = program.brand(_FOO_DESIGN_ID, ..., export_oem_info = True)
```
