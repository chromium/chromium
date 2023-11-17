# Generating PGO Profiles

Normally devs don't need to worry about this and can use the default profile
for official builds.  The default profile can be fetched by adding
`"checkout_pgo_profiles": True` to `custom_vars` in the gclient config and
running `gclient runhooks`.

To produce an executable built with a custom PGO profile:

* Produce the instrumented executable using the following gn args:

  ```
  chrome_pgo_phase = 1
  enable_resource_allowlist_generation = false
  is_official_build = true
  symbol_level = 0
  use_remoteexec = true
  ```

  For android you may need in addition:
  ```
  target_os = "android"
  target_cpu = "arm64"
  ```

* Run representative benchmarks to produce profiles

  `python3 tools/pgo/generate_profile.py -C out/builddir`

  If collecting profiles on an android device, add the following extra args
  (a browser name like [these][browser_names] and the correct cache dir):

  ```
  python3 tools/pgo/generate_profile.py -C out/builddir \
      --android-browser android-trichrome-bundle
      --android-device-path /data/data/com.google.android.apps.chrome/cache/pgo_profiles
  ```

  If this fails with `ServiceException: 401 Anonymous caller does not have storage.objects.get
  access to the Google Cloud Storage object.`, then run `download_from_google_storage --config`
  (with your @google address; enter 0 as project-id).

  This will produce `out/builddir/prof.profdata`

* Produce the final PGO'd executable with the following gn args (and additional
  android args, if any):

  ```
  enable_resource_allowlist_generation = false
  is_official_build = true
  symbol_level = 0
  use_remoteexec = true
  pgo_data_path = "//out/builddir/prof.prodata"
  ```

[browser_names]: https://source.chromium.org/chromium/chromium/src/+/main:tools/perf/core/perf_json_config_validator.py;l=124;drc=dca3d88bd5c0d67cedf0796f007ed6258b1b827d