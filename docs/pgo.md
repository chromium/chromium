# Profile-Guided Optimization (PGO)

## Generating PGO Profiles via Bots

See [go/chrome-pgo-internal] (Googlers only).

[go/chrome-pgo-internal]: https://goto.google.com/chrome-pgo-internal

## Generating PGO Profiles Manually

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

  For android you need these in addition:
  ```
  target_os = "android"
  target_cpu = "arm64"
  ```

* Run representative benchmarks to produce profiles

  `python3 tools/pgo/generate_profile.py -C out/builddir`

  If collecting profiles on an android device, add a browser name like one of
  [these][browser_names]:

  ```
  python3 tools/pgo/generate_profile.py -C out/builddir \
      --android-browser android-trichrome-bundle
  ```

  You can find available browsers using:

  ```
  tools/perf/run_benchmark run --browser=list
  ```

  By default, some benchmark replay archives require special access permissions. For more
  details and to request access, please refer to [Telemetry documentation][telemetry_docs].
  You can also choose to run `generate_profile.py` without these benchmarks, using the
  `--run-public-benchmarks-only` flag. However, note that doing so may produce a profile
  that isn't fully representative.

   ```
  python3 tools/pgo/generate_profile.py -C out/builddir \
      --android-browser android-trichrome-bundle \
      --run-public-benchmarks-only
  ```

  If `generate_profile.py` fails with `ServiceException: 401 Anonymous caller does not have
  storage.objects.get access to the Google Cloud Storage object.`, then run
  `download_from_google_storage --config` (with your @google address; enter 0 as
  project-id).

  This will produce `out/builddir/profile.profdata`

* Produce the final PGO'd executable with the following gn args (and additional
  android args, if any):

  ```
  enable_resource_allowlist_generation = false
  is_official_build = true
  symbol_level = 0
  use_remoteexec = true
  pgo_data_path = "//out/builddir/profile.profdata"
  ```

[browser_names]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/catapult/telemetry/telemetry/internal/backends/android_browser_backend_settings.py;l=400;drc=bf85e76dc3467385a623e9bf11ab950cf2889ca5
[telemetry_docs]: https://www.chromium.org/developers/telemetry/upload_to_cloud_storage/#request-access-for-google-partners

## How It Works

`chrome_pgo_phase` is defined in [`build/config/compiler/pgo/pgo.gni`][pgo_gni].
This GN variable can be one of 0, 1, or 2, meaning "don't use profile",
"generating profile", and "use profile" respectively. See [pgo.gni][pgo_gni] for
details on platform-specific GN variables that determine which phase is used in
each build.

Which file under `//chrome/build/pgo_profiles/` gets used? It depends on both
the platform and [`_pgo_target`][pgo_target]. For example, for 64-bit android,
the file `//chrome/build/android-arm64.pgo.txt` contains the name of the
`*.profdata` file that is used as the PGO profile by default if no other profile
is specified via the GN arg `pgo_data_path`.

[pgo_gni]: https://source.chromium.org/chromium/chromium/src/+/main:build/config/compiler/pgo/pgo.gni
[pgo_target]: https://source.chromium.org/chromium/chromium/src/+/main:build/config/compiler/pgo/BUILD.gn;l=88;drc=3d2e089ad74a30754376571531e00615de96061e

## Background Reading

https://clang.llvm.org/docs/UsersManual.html#profile-guided-optimization

https://source.android.com/docs/core/perf/pgo
