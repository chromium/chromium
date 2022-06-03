#!/usr/bin/perl -wT
# Produces a text/javascript larger than 1K. On the first load responds with 200
# with a validator. On subsequent requests with revalidation responds with 304.

use strict;

print <<EOM;
Content-Type: text/javascript
Access-Control-Allow-Origin: *
Cache-Control: no-cache, must-revalidate
Last-Modified: Mon, 16 Mar 2020 22:00:00 GMT

// So sorry about this waste of bytes:
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
// Filler comment, to trigger code caching heuristic (script > 1K.)
EOM
