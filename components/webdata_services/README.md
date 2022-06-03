The webdata services component contains the wrappers used to access the specific
services built atop the web database (see //components/webdata/).  Because there
is a single database instance, the various services accessing different tables
are created and destroyed together, and this component is what does that tying
together.
