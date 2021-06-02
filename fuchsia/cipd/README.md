# CIPD recipes

The `//fuchsia/cipd` target generates a number of YAML files which are used to
produce archives that are uploaded to CIPD. The generated YAML files are stored
under the output directory, under the path `gen/fuchsia/cipd/`.

## Example usage

The most recent package can be discovered by searching for the "canary" ref:

`$ cipd describe chromium/fuchsia/$PACKAGE_NAME-$TARGET_ARCH -version canary`
