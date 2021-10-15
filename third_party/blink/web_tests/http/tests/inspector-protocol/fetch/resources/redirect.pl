#!/usr/bin/perl

print "Status: 302 Found\r\n";
print "Location: $ENV{'QUERY_STRING'}\r\n\r\n";
