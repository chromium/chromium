# CIPD recipes

The `//fuchsia/cipd` target generates a number of YAML files which are used to
produce archives that are uploaded to CIPD. The generated YAML files are stored
under the output directory, under the path `gen/fuchsia/cipd/`.

## Example usage

To create a CIPD package, run the following command from the build output
directory. In this example, "http.yaml" is being built for arm64:

```
$ cipd create --pkg-def gen/fuchsia/cipd/http/http.yaml
              -ref latest
              -tag version:$(cat gen/fuchsia/cipd/build_id.txt)
```

The most recent package can be discovered by searching for the "latest" ref:

`$ cipd describe chromium/fuchsia/$PACKAGE_NAME-$TARGET_ARCH -version latest`
