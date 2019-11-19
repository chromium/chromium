# Layered API resources directory

This directory stores module JavaScript implementation files of
[Layered APIs](https://github.com/drufball/layered-apis),
that are to be bundled with the browser.

## Adding/removing files

When adding/removing files, execute

- `core/script/generate_lapi_grdp.py`
- `git cl format`

to update the following files:

- `core/script/layered_api_resources.h`
- `core/script/resources/layered_api/resources.grdp`

and commit these files together with the changes under resources/layered_api/.

(This is not needed when the content of the files are modified)

## Which files are bundled

All files under

- Sub-directories which have 'index.mjs' or
- Directories of which last path component is 'internal'

will be included in the grdp and thus bundled in the Chrome binary,
except for files starting with '.', 'README', or 'OWNERS'.

So be careful about binary size increase when you add new files or add more
contents to existing files.

## What are exposed

All bundled resources are mapped to `std-internal://path-relative-to-here`, and
`std-internal:` resources are not accessible from the web.  Resources loaded as
`std-internal:` can import other `std-internal:` resources.

For example, `layered_api/foo/bar/baz.mjs` is mapped to
`std-internal://foo/bar/baz.mjs`.

All `index.mjs` resources are mapped to `std:directory-name-relative-to-here`
too, and they are web-exposed.  For example,
`layered_api/elements/toast/index.mjs` is mapped to `std:elements/toast` as
well as `std-internal://elements/toast/index.mjs`.
