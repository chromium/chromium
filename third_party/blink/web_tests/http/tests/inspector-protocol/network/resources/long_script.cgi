#!/usr/bin/perl -wT
use strict;

my (@out, $i, $line);
push(@out, "Content-type: text/plain\n");
push(@out, "\n");
$line = "// " . "-" x 1020 . "\n";
for ($i = 0; $i < 10240; $i++) {
    push(@out, $line);
}
push(@out, "console.log('finished');\n");
print join("", @out);
