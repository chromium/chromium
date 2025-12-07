The webdata component manages the "web database", a SQLite database stored in
the user's profile containing various webpage-related metadata such as autofill
and web search engine data. A single database (per profile) is shared by many
unrelated features.

**If you are adding data storage for a [new] feature and want to use SQLite, you
should probably default to creating a unique database using the compat layer in
[`//sql`](https://source.chromium.org/chromium/chromium/src/+/main:sql/).**
See [SQLite is a local optimum for Chromium](https://bit.ly/3Xc3EcH).

This component is not allowed to depend on content/, because it is used by iOS.
If dependencies on content/ need to be added, this component will have to be
made into a layered component: see
https://www.chromium.org/developers/design-documents/layered-components-design .
