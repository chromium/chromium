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
  use_goma = true
  ```

* Run representative benchmarks to produce profiles

  `python3 tools/pgo/generate_profile.py -C out/builddir1`

  If this fails with `ServiceException: 401 Anonymous caller does not have storage.objects.get
  access to the Google Cloud Storage object.`, then run `download_from_google_storage --config`
  (with your @google address; enter 0 as project-id).

  This will produce `out/builddir1/prof.profdata`

* Produce the final PGO'd executable with the following gn args:

  ```
  enable_resource_allowlist_generation = false
  is_official_build = true
  symbol_level = 0
  use_goma = true
  pgo_data_path = "//out/builddir1/prof.prodata"
  ```
