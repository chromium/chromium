all: \
	architecture.png

%.png: %.gv Makefile
	dot -Tpng $< -o $@
