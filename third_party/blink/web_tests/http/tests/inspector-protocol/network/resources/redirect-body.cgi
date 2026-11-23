#!/usr/bin/perl -wT
use strict;

my ($method, @pairs, $pair, %params, $ttl, $status);
$method = $ENV{'REQUEST_METHOD'};
$method =~ tr/a-z/A-Z/;

@pairs = split(/&/, $ENV{'QUERY_STRING'});

foreach $pair (@pairs)
{
    my ($name, $value);
    ($name, $value) = split(/=/, $pair);
    $params{$name} = $value;
}

$ttl = int($params{'ttl'});
$status = $params{'status'};

if ($ttl > 0) {
    $ttl = $ttl - 1;
    print "Status: $status Lorem ipsum\n";
    print "Location: redirect-body.cgi?status=$status&ttl=$ttl\n";
}

print "request-method: $method\n";
print "\n";
