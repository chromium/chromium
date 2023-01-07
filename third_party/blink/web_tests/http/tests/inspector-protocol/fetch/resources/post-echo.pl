#!/usr/bin/perl
print "Cache-control: no-cache\r\n";
print "Content-type: text/plain\r\n\r\n";
if ($ENV{'REQUEST_METHOD'} eq "POST") {
    read(STDIN, $request, $ENV{'CONTENT_LENGTH'})
                || die "Could not get query\n";
    print $request;
} else {
    print "Wrong method: " . $ENV{'REQUEST_METHOD'} . "\n";
}
