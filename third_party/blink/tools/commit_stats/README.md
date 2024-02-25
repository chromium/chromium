This directory contains scripts to collect statistics from the codebase, mostly
for use in the BlinkOn keynote presentations.

git-dirs.txt and org-list.txt are manually curated, and kept up-to-date by those
leveraging the analyses. The scripts are intended for processing repositories of
projects which are critical to Chromium and have supporting Chromium as a
primary goal, although this is a somewhat blurry line (eg. includes the VP8
codec but not the JPEG codec). 

We consider "chromium.org", "webrtc.org" and "skia.org" addresses to be
"Google", unless they have a more explicit affiliation defined in
`affiliations.json5`.
