#!/bin/bash
for email in $( \
  git log \
    --pretty=format:"%ae" | head -n 100); do
  pattern=`echo $email | awk -F@ '{print $2}'`
  # TODO - just try out these lines separately from the rest of the script!!
  echo $pattern
  echo $email
done

