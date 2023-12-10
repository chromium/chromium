This directory defines the Chrome-facing interface to the ChromeML shared
library, along with client-side types to simplify its usage.

Chrome may statically link against code in this directory, and code here may
depend on other parts of the Chromium tree (e.g. //base).

Code here may NOT depend on anything else in
//components/optimization_guide/internal, except indirectly through the
ChromeMLAPI boundary.
