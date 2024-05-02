# Use the official Ubuntu image as the base
FROM debian:12

# Install any necessary dependencies
RUN apt-get update && apt-get install -y \
    curl python3-dev git \
    xz-utils \
    pkgconf \
    libglib2.0-0 \
    libnss3 \
    && rm -rf /var/lib/apt/lists/*

ADD files/sccache /usr/local/bin

ENV SCCACHE_GCS_BUCKET=githubactions-chrome-sccache
ENV SCCACHE_GCS_RW_MODE=READ_WRITE
ENV SCCACHE_GCS_KEY_PATH=/root/key.json
# ENV SCCACHE_GCS_SERVICE_ACCOUNT=github-actions-bucket-access@cobalt-demo-330821.iam.gserviceaccount.com

USER chrobalt
ENV HOME /home/chrobalt
