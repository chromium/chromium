set -ex
autoninja -C out/Default chrome
./out/Default/chrome --no-sandbox

# ./out/Default/chrome --no-sandbox --enable-logging --log-level=0 --v=1 &> logs