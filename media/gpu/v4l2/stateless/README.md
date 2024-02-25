The `V4L2StatelessVideoDecoder` implemented in this directory is meant as
a replacement for the one in the parent directory. This complements the
flattening of the stateful decoder (b/297565040). The initial implementation
is meant to be only for stateless in order to avoid breaking the current
implementation. There is a desire to share code with the stateful in the
future.
