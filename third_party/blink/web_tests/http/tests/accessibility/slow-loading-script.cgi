#!/usr/bin/perl -wT
use Time::HiRes;

Time::HiRes::sleep(1.5);
print "Content-type: application/octet-stream\n";
print "Cache-control: no-cache, no-store\n\n";
