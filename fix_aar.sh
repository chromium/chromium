#!/usr/bin/env bash

export AAR=$1
aar_name=$(basename ${AAR})
aar_name=${aar_name%%.*}
echo "AAR_NAME=${aar_name}"
export EXTRACT_DIR=./${aar_name}_extracted
export OUTPUT=${aar_name}.aar
rm -rf $EXTRACT_DIR && mkdir $EXTRACT_DIR
unzip $AAR -d $EXTRACT_DIR
cp -r $EXTRACT_DIR/res/0_res/* $EXTRACT_DIR/res
rm -rf $EXTRACT_DIR/res/0_res
# Rename files that have _0 or _1 suffix
find $EXTRACT_DIR/res -type f | sed -n 's/^\(.*\)_[01]\.\([[:alpha:]]*\)$/mv & \1.\2/p' | sh
rm -f $OUTPUT
pushd $EXTRACT_DIR
zip -r $OUTPUT *
mv $OUTPUT ../
popd

