This directory contains scripts and metadata to collect commit statistics from
the codebase. Historically this was used for BlinkOn keynote presentations.
Since 2024 we've instead used https://chrome-commit-tracker.arthursonzogni.com/,
which itself relies on the affiliations.json5 file here.

git-dirs.txt and org-list.txt are manually curated, and kept up-to-date by those
leveraging the analyses. The scripts are intended for processing repositories of
projects which are critical to Chromium and have supporting Chromium as a
primary goal, although this is a somewhat blurry line (eg. includes the VP8
codec but not the JPEG codec).

We consider "chromium.org", "webrtc.org" and "skia.org" addresses to be
"Google", unless they have a more explicit affiliation defined in
`affiliations.json5`.
