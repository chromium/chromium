The webdata component manages the "web database", a SQLite database stored in
the user's profile containing various webpage-related metadata such as autofill
and web search engine data.

This component is not allowed to depend on content/, because it is used by iOS.
If dependencies on content/ need to be added, this component will have to be
made into a layered component: see
https://www.chromium.org/developers/design-documents/layered-components-design .
