## What are watchlists?

A watchlist is a mechanism that allows a developer (a "watcher") to watch over
portions of code that the watcher is interested in. A watcher will be cc-ed on
changes that modify that portion of code, thereby giving that watcher an
opportunity to make comments on chromium-review.googlesource.com even before the
change is committed.

## Syntax

Watchlists are defined using a `WATCHLISTS` file, which resides at the root of a
repository. A typical `WATCHLISTS` file looks like:

```
{
  'WATCHLIST_DEFINITIONS': {
    'valgrind': {
      'filepath': 'tools/valgrind/',
    },
    'mac': {
      'filepath': 'cocoa|\.mm$|(_mac|_posix)\.(cc|h)$',
    },
  },
  'WATCHLISTS': {
    'valgrind': ['nirnimesh@chromium.org', 'dank@chromium.org'],
  },
}
```

In this case, watchlists named `valgrind` and `mac` are defined in
`WATCHLIST_DEFINITIONS` and their corresponding watchers declared in
`WATCHLISTS`.

In the example above, whenever a new changeset is created that refers to any
file in `tools/valgrind/`, the `'valgrind'` watchlist will be triggered and
`nirnimesh@chromium.org` & `dank@chromium.org` will be cc-ed to the changeset
for review. A regular expression can be used as the matching pattern. Matches
are determined using python's `re.search()` function call, so matching `A_WORD`
is the same as matching `.*A_WORD.*`.

Each name in `WATCHLISTS` must be defined first in `WATCHLIST_DEFINITIONS`.

Watchlist processing takes place on Gerrit with the "Watchlists" analyzer and is
non-binding; that is, an approval from that watcher is not needed for commit. It
merely gives the watcher an opportunity to make comments, if any.

## Editing Watchlists

You create new watchlists or add yourself to existing watchlists by editing the
WATCHLISTS file at the base of the repository.

It's advisable to run `watchlists.py` to verify that your new rules work.

Example (from src):

```
python third_party/depot_tools/watchlists.py PATH/TO/FILE1 PATH/TO/FILE2 ....
```
