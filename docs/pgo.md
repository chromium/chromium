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

  * `vpython3 tools/perf/run_benchmark system_health.common_desktop --assert-gpu-compositing --run-abridged-story-set --browser=exact --browser-executable=out/path/to/chrome`
  * `vpython3 tools/perf/run_benchmark speedometer2 --assert-gpu-compositing --browser=exact --browser-executable=out/path/to/chrome`
  * `vpython3 tools/perf/run_benchmark rendering.desktop --story-tag-filter=motionmark_fixed_2_seconds --also-run-disabled-tests --assert-gpu-compositing --browser=exact --browser-executable=out/path/to/chrome`
  * `vpython3 tools/perf/run_benchmark jetstream2 --assert-gpu-compositing --browser=exact --browser-executable=out/path/to/chrome`
  * This will produce `*.profraw` files in the current working directory

  If this fails with `ServiceException: 401 Anonymous caller does not have storage.objects.get
  access to the Google Cloud Storage object.`, then run `download_from_google_storage --config`
  (with your @google address; enter 0 as project-id).

* Merge the profiling data

  * Get the `llvm-profdata` tool by adding `"checkout_clang_coverage_tools": True,` to `custom_vars` in the gclient config and running `gclient runhooks`.
  * Run `third_party/llvm-build/Release+Asserts/bin/llvm-profdata merge *.profraw -o chrome.profdata`

* Produce the final PGO'd executable with the following gn args:

  ```
  enable_resource_allowlist_generation = false
  is_official_build = true
  symbol_level = 0
  use_goma = true
  pgo_data_path = {path-to-the-profile}
  ```
